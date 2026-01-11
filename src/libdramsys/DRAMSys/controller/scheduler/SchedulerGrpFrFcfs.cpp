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
    grpBuffer[0] = {ControllerVector<Bank, std::list<AgePayload>>(memSpec.banksPerChannel), AgeCounter(config.maxAgeWaited)};
    grpBuffer[1] = {ControllerVector<Bank, std::list<AgePayload>>(memSpec.banksPerChannel), AgeCounter(config.maxAgeWaited)};

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
  int idx = toBufIdx(payload.get_command());
  trans.set_born(grpBuffer[idx].age.get_year());
  grpBuffer[idx].buffer[ControllerExtension::getBank(payload)].push_back(trans);

  bufferCounter->storeRequest(payload);
}

void SchedulerAgeGrpFrFcfs::removeRequest(tlm_generic_payload& payload)
{
  bufferCounter->removeRequest(payload);
  lastCommand = payload.get_command();
  Bank bank = ControllerExtension::getBank(payload);

  AgePayload trans = AgePayload(&payload);
  grpBuffer[toBufIdx(payload.get_command())].buffer[bank].remove(trans);
}

//@jg 1-11
using namespace std;

tlm_generic_payload* SchedulerAgeGrpFrFcfs::getNextRequest(const BankMachine& bankMachine) const
{
  // search row hits, search wrd/wr hits
  // search rd/wr hits, search row hits
  Bank bank = bankMachine.getBank();

  int idx = toBufIdx(lastCommand);

  cout << "bank = " << int(bank) << ", idx = " << idx << endl;
  cout << "year = " << grpBuffer[idx].age.get_year() << endl;
  if (!grpBuffer[idx].buffer[bank].empty()) {
    if (bankMachine.isActivated()) {
      // Search for read row hit
      Row openRow = bankMachine.getOpenRow();
      //@jg 1-11
      std::cout << "openRow = " << int(openRow) << std::endl;
      for (auto it : grpBuffer[idx].buffer[bank]) {
        if (it.is_ergent()) {
          //cout << "is_ergent: A = " << tlm_generic_payload *(it)->get_address() << endl;
          return it;
        }
        if (grpBuffer[idx].age.is_aged(it.get_born())) {
          it.set_ergent(true);
          cout << "set_ergent.\n";
        }
        if (ControllerExtension::getRow(*it) == openRow) {
          //cout << "row-hit: A = " << (tlm_generic_payload *)(it)->get_address() << endl;
          grpBuffer[idx].age.ageInc();
          cout << "year++\n";
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
