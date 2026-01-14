/*
 * Copyright (c) 2019, RPTU Kaiserslautern-Landau
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
 */

#pragma once

#include <tlm>
#include <ostream>
#include <sstream>
#include "DRAMSys/common/DebugManager.h"

using namespace tlm;
using namespace std;

namespace DRAMSys
{
/*
 * maxAge (> 0) is the mode of year.
 * Call is_Aged() before every time calling ageInc();
 * This make sure year only turn around maxAge one time.
 */
class AgeCounter {
 public:
  AgeCounter() {
    year = maxAge = 0;
  }

  AgeCounter(int max) {
    year = 0;
    maxAge = (max > 0) ? max : 0;
  }

  int get_year() {
    return year;
  }

  int get_maxAge() {
    return maxAge;
  }

  void ageInc() {
    year++;
    if ((maxAge > 0) && (year == maxAge))
      year = 0;
  }

  bool is_aged(int born) const {
    if (maxAge > 0) {
      int age;
      if (year > born) {
        age = year - born;
      } else {
        age = year + maxAge - born;
      }
      return (age == maxAge);
    }
    return false;
  }

 private:
  int year;
  int maxAge;
};

/*
 * Scheduler's year increased each dequeue.
 *
 * Set Payload.born to scheduler's year when enqueue,
 * then check whether the distance between born and year equals to maxAge;
 * When they are equal, dequeue it other than row-hit.
 *
 * If there are more than 1 candidate payload (abs(year-born) == maxAge), set is_ergent flag;
 * Dequeue ergent Payload next time other than row-hit.
 */
class AgePayload {
 public:
  explicit AgePayload(tlm_generic_payload *trans) {
    payload = trans;
    born = 0;
    ergent = false;
  }

/*
  AgePayload(const AgePayload& tran) {
    payload = tran.payload;
    born = tran.born;
    ergent = tran.ergent;
    stringstream ss;
    ss << *this;
    PRINTDEBUGMESSAGE("AgePayload(const tran)", ss.str());
  }

  ~AgePayload() {
    stringstream ss;
    ss << *this;
    PRINTDEBUGMESSAGE("~AgePayload()", ss.str());
  }
*/
  operator tlm_generic_payload *() const {
    return payload;
  }

  bool operator == (const AgePayload& other) const {
    return payload == other.payload;
  }

  friend bool operator == (const AgePayload& left, const AgePayload& right) {
    return left.operator ==(right);
  }

  bool operator != (const AgePayload& other) const {
    return payload != other.payload;
  }

  friend bool operator != (const AgePayload& left, const AgePayload& right) {
    return left.operator ==(right);
  }

  friend ostream& operator << (ostream& os, const AgePayload& tran) {
    os << "AgePayload:{" << hex << long(tran.payload) << "(" << tran.payload->get_address() << "),"
        << tran.born << ","
        << tran.ergent << "}.";
    return os;
  }

  void set_born(int now) { born = now; }
  int get_born() { return born; }
  void set_ergent(bool state) { ergent = state; }
  bool is_ergent() { return ergent; }
 private:
  tlm_generic_payload *payload;
  int born;
  bool ergent;
};
} // namespace DRAMSys
