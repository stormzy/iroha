/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_MST_PASSED_RESPONSE_HPP
#define IROHA_MST_PASSED_RESPONSE_HPP

namespace shared_model {
  namespace interface {
    /**
     * Transaction successfully passed MST and is ready for stateful validation
     */
    class MstPassedResponse : public AbstractTxResponse<MstPassedResponse> {
     private:
      std::string className() const override {
        return "MstPassedResponse";
      }
    };
  }  // namespace interface
}  // namespace shared_model

#endif  // IROHA_MST_PASSED_RESPONSE_HPP
