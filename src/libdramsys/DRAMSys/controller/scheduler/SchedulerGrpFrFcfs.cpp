/*
 * Copyright (c) 2022, RPTU Kaiserslautern-Landau
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Lukas Steiner
 */

#include "SchedulerGrpFrFcfs.h"

#include "DRAMSys/controller/scheduler/BufferCounterBankwise.h"
#include "DRAMSys/controller/scheduler/BufferCounterReadWrite.h"
#include "DRAMSys/controller/scheduler/BufferCounterShared.h"

using namespace tlm;

namespace DRAMSys
{

SchedulerGrpFrFcfs::SchedulerGrpFrFcfs(const McConfig& config, const MemSpec& memSpec)
{
    readBuffer = ControllerVector<Bank, std::list<tlm_generic_payload*>>(memSpec.banksPerChannel);
    writeBuffer = ControllerVector<Bank, std::list<tlm_generic_payload*>>(memSpec.banksPerChannel);

    if (config.schedulerBuffer == Config::SchedulerBufferType::Bankwise)
        bufferCounter = std::make_unique<BufferCounterBankwise>(config.requestBufferSize,
                                                                memSpec.banksPerChannel);
    else if (config.schedulerBuffer == Config::SchedulerBufferType::ReadWrite)
        bufferCounter = std::make_unique<BufferCounterReadWrite>(config.requestBufferSizeRead,
                                                                 config.requestBufferSizeWrite);
    else if (config.schedulerBuffer == Config::SchedulerBufferType::Shared)
        bufferCounter = std::make_unique<BufferCounterShared>(config.requestBufferSize);

    SC_REPORT_WARNING("SchedulerGrpFrFcfs", "Hazard detection not yet implemented!");
}

bool SchedulerGrpFrFcfs::hasBufferSpace(unsigned entries) const
{
    return bufferCounter->hasBufferSpace(entries);
}

void SchedulerGrpFrFcfs::storeRequest(tlm_generic_payload& payload)
{
    if (payload.is_read())
        readBuffer[ControllerExtension::getBank(payload)].push_back(&payload);
    else
        writeBuffer[ControllerExtension::getBank(payload)].push_back(&payload);
    bufferCounter->storeRequest(payload);
}

void SchedulerGrpFrFcfs::removeRequest(tlm_generic_payload& payload)
{
    bufferCounter->removeRequest(payload);
    lastCommand = payload.get_command();
    Bank bank = ControllerExtension::getBank(payload);

    if (payload.is_read())
        readBuffer[bank].remove(&payload);
    else
        writeBuffer[bank].remove(&payload);
}

tlm_generic_payload* SchedulerGrpFrFcfs::getNextRequest(const BankMachine& bankMachine) const
{
    // search row hits, search wrd/wr hits
    // search rd/wr hits, search row hits
    Bank bank = bankMachine.getBank();

    if (lastCommand == tlm::TLM_READ_COMMAND)
    {
        if (!readBuffer[bank].empty())
        {
            if (bankMachine.isActivated())
            {
                // Search for read row hit
                Row openRow = bankMachine.getOpenRow();
                for (auto* it : readBuffer[bank])
                {
                    if (ControllerExtension::getRow(*it) == openRow)
                        return it;
                }
            }
            // No read row hit found or bank precharged
            return readBuffer[bank].front();
        }
        if (!writeBuffer[bank].empty())
        {
            if (bankMachine.isActivated())
            {
                // Search for write row hit
                Row openRow = bankMachine.getOpenRow();
                for (auto* it : writeBuffer[bank])
                {
                    if (ControllerExtension::getRow(*it) == openRow)
                        return it;
                }
            }
            // No write row hit found or bank precharged
            return writeBuffer[bank].front();
        }
        return nullptr;
    }

    if (!writeBuffer[bank].empty())
    {
        if (bankMachine.isActivated())
        {
            // Search for write row hit
            Row openRow = bankMachine.getOpenRow();
            for (auto* it : writeBuffer[bank])
            {
                if (ControllerExtension::getRow(*it) == openRow)
                    return it;
            }
        }
        // No write row hit found or bank precharged
        return writeBuffer[bank].front();
    }
    if (!readBuffer[bank].empty())
    {
        if (bankMachine.isActivated())
        {
            // Search for read row hit
            Row openRow = bankMachine.getOpenRow();
            for (auto* it : readBuffer[bank])
            {
                if (ControllerExtension::getRow(*it) == openRow)
                    return it;
            }
        }
        // No read row hit found or bank precharged
        return readBuffer[bank].front();
    }
    return nullptr;
}

bool SchedulerGrpFrFcfs::hasFurtherRowHit(Bank bank, Row row, tlm_command command) const
{
    // TODO: do this based on current RD/WR mode
    unsigned rowHitCounter = 0;
    if (command == tlm::TLM_READ_COMMAND)
    {
        for (auto* it : readBuffer[bank])
        {
            if (ControllerExtension::getRow(*it) == row)
            {
                rowHitCounter++;
                if (rowHitCounter == 2)
                    return true;
            }
        }
        return false;
    }

    for (auto* it : writeBuffer[bank])
    {
        if (ControllerExtension::getRow(*it) == row)
        {
            rowHitCounter++;
            if (rowHitCounter == 2)
                return true;
        }
    }

    return false;
}

bool SchedulerGrpFrFcfs::hasFurtherRequest(Bank bank, tlm_command command) const
{
    if (command == tlm::TLM_READ_COMMAND)
    {
        return readBuffer[bank].size() >= 2;
    }

    return writeBuffer[bank].size() >= 2;
}

const std::vector<unsigned>& SchedulerGrpFrFcfs::getBufferDepth() const
{
    return bufferCounter->getBufferDepth();
}


SchedulerAgeGrpFrFcfs::SchedulerAgeGrpFrFcfs(const McConfig& config, const MemSpec& memSpec)
{
    grpBuffer[READ_COMMAND] = {ControllerVector<Bank, std::list<AgePayload>>(memSpec.banksPerChannel), ControllerVector<Bank, AgeCounter>(memSpec.banksPerChannel, config.maxAgeWaited)};
    grpBuffer[WRITE_COMMAND] = {ControllerVector<Bank, std::list<AgePayload>>(memSpec.banksPerChannel), ControllerVector<Bank, AgeCounter>(memSpec.banksPerChannel, config.maxAgeWaited)};

    if (config.schedulerBuffer == Config::SchedulerBufferType::Bankwise)
        bufferCounter = std::make_unique<BufferCounterBankwise>(config.requestBufferSize,
                                                                memSpec.banksPerChannel);
    else if (config.schedulerBuffer == Config::SchedulerBufferType::ReadWrite)
        bufferCounter = std::make_unique<BufferCounterReadWrite>(config.requestBufferSizeRead,
                                                                 config.requestBufferSizeWrite);
    else if (config.schedulerBuffer == Config::SchedulerBufferType::Shared)
        bufferCounter = std::make_unique<BufferCounterShared>(config.requestBufferSize);

    SC_REPORT_WARNING("SchedulerAgeGrpFrFcfs", "Row-hit prefered on Read/Write queues with age counter.");
}

bool SchedulerAgeGrpFrFcfs::hasBufferSpace(unsigned entries) const
{
    return bufferCounter->hasBufferSpace(entries);
}

void SchedulerAgeGrpFrFcfs::storeRequest(tlm_generic_payload& payload)
{
  AgePayload trans = AgePayload(&payload);
  auto idx = toBufIdx(payload.get_command());
  Bank bank = ControllerExtension::getBank(payload);
  trans.set_born(grpBuffer[idx].age[bank].get_year());
  grpBuffer[idx].buffer[bank].push_back(trans);

  bufferCounter->storeRequest(payload);
  {
    stringstream ss;
    ss << trans;
    PRINTDEBUGMESSAGE("storeRequest():", ss.str());
  }
}

void SchedulerAgeGrpFrFcfs::removeRequest(tlm_generic_payload& payload)
{
  bufferCounter->removeRequest(payload);
  lastCommand = payload.get_command();
  auto idx = toBufIdx(payload.get_command());
  Bank bank = ControllerExtension::getBank(payload);

  AgePayload trans = AgePayload(&payload);
  grpBuffer[idx].buffer[bank].remove(trans);
  grpBuffer[idx].age[bank].ageInc();
  {
    stringstream ss;
    ss << trans << endl
        << "age[" << int(bank) << "] ++";
    PRINTDEBUGMESSAGE("removeRequest():", ss.str());
  }
}

tlm_generic_payload* SchedulerAgeGrpFrFcfs::getNextRequest(const BankMachine& bankMachine) const
{
  // search row hits, search wrd/wr hits
  // search rd/wr hits, search row hits
  Bank bank = bankMachine.getBank();

  auto idx = toBufIdx(lastCommand);

  if (!grpBuffer[idx].buffer[bank].empty()) {
    if (bankMachine.isActivated()) {
      // Search for read row hit
      Row openRow = bankMachine.getOpenRow();
      for (auto& it : grpBuffer[idx].buffer[bank]) {
        if (it.is_ergent()) {
          return it;
        }
        if (grpBuffer[idx].age[bank].is_aged(it.get_born())) {
          it.set_ergent(true);
          {
            stringstream ss;
            ss << "set_ergent( " << it << ").";
            PRINTDEBUGMESSAGE("getNextRequest():", ss.str());
          }
        }
        if (ControllerExtension::getRow(*it) == openRow) {
          return it;
        }
      }
    }
    // No row hit found or bank precharged
    return grpBuffer[idx].buffer[bank].front();
  }

  return nullptr;
}

bool SchedulerAgeGrpFrFcfs::hasFurtherRowHit(Bank bank, Row row, tlm_command command) const
{
    // TODO: do this based on current RD/WR mode
    unsigned rowHitCounter = 0;
    if (command == tlm::TLM_READ_COMMAND)
    {
        for (auto it : grpBuffer[0].buffer[bank])
        {
            if (ControllerExtension::getRow(*it) == row)
            {
                rowHitCounter++;
                if (rowHitCounter == 2)
                    return true;
            }
        }
        return false;
    }

    for (auto it : grpBuffer[1].buffer[bank])
    {
        if (ControllerExtension::getRow(*it) == row)
        {
            rowHitCounter++;
            if (rowHitCounter == 2)
                return true;
        }
    }

    return false;
}

bool SchedulerAgeGrpFrFcfs::hasFurtherRequest(Bank bank, tlm_command command) const
{
    if (command == tlm::TLM_READ_COMMAND)
    {
        return grpBuffer[0].buffer[bank].size() >= 2;
    }

    return grpBuffer[1].buffer[bank].size() >= 2;
}

const std::vector<unsigned>& SchedulerAgeGrpFrFcfs::getBufferDepth() const
{
    return bufferCounter->getBufferDepth();
}
} // namespace DRAMSys
