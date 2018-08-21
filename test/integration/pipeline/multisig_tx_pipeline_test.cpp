/**
 * Copyright Soramitsu Co., Ltd. 2018 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "builders/protobuf/queries.hpp"
#include "cryptography/crypto_provider/crypto_defaults.hpp"
#include "framework/integration_framework/integration_test_framework.hpp"
#include "framework/specified_visitor.hpp"
#include "integration/acceptance/acceptance_fixture.hpp"

using namespace std::string_literals;
using namespace integration_framework;
using namespace shared_model;

class MstPipelineTest : public AcceptanceFixture {
 public:
  /**
   * Sign the transaction
   * @param tx pre-built transaction
   * @param key to sign the transaction
   * @return signed transaction
   */
  template <typename TxBuilder>
  auto signTx(TxBuilder tx, const crypto::Keypair &key) const {
    return tx.build().signAndAddSignature(key).finish();
  }

  /**
   * Creates a mst user
   * @param itf, in which the user will be created
   * @param sigs - number of signatories of that mst user
   * @return itf with created user
   */
  IntegrationTestFramework &makeMstUser(IntegrationTestFramework &itf,
                                        size_t sigs = kSignatories) {
    auto create_user_tx =
        createUserWithPerms(
            kUser,
            kUserKeypair.publicKey(),
            kNewRole,
            {shared_model::interface::permissions::Role::kSetQuorum,
             shared_model::interface::permissions::Role::kAddSignatory,
             shared_model::interface::permissions::Role::kSetDetail})
            .build()
            .signAndAddSignature(kAdminKeypair)
            .finish();
    auto add_signatories_tx = baseTx().quorum(1);
    for (size_t i = 0; i < sigs; ++i) {
      signatories.push_back(
          crypto::DefaultCryptoAlgorithmType::generateKeypair());
      add_signatories_tx =
          add_signatories_tx.addSignatory(kUserId, signatories[i].publicKey());
    }
    add_signatories_tx.setAccountQuorum(kUserId, sigs + 1);
    itf.sendTx(create_user_tx)
        .checkProposal([](auto &proposal) {
          ASSERT_EQ(proposal->transactions().size(), 1);
        })
        .checkVerifiedProposal([](auto &proposal) {
          ASSERT_EQ(proposal->transactions().size(), 1);
        })
        .checkBlock([](auto &proposal) {
          ASSERT_EQ(proposal->transactions().size(), 1);
        })
        .sendTx(add_signatories_tx.build()
                    .signAndAddSignature(kUserKeypair)
                    .finish())
        .checkProposal([](auto &proposal) {
          ASSERT_EQ(proposal->transactions().size(), 1);
        })
        .checkVerifiedProposal([](auto &proposal) {
          ASSERT_EQ(proposal->transactions().size(), 1);
        })
        .checkBlock([](auto &proposal) {
          ASSERT_EQ(proposal->transactions().size(), 1);
        });
    return itf;
  }

  /**
   * Makes a ready-to-send query to get pending transactions
   * @param creator - account, which asks for pending transactions
   * @param key - that account's keypair
   * @return built and signed transaction
   */
  auto makeGetPendingTxsQuery(const std::string &creator,
                              const crypto::Keypair &key) {
    return shared_model::proto::QueryBuilder()
        .createdTime(iroha::time::now())
        .creatorAccountId(creator)
        .queryCounter(1)
        .getPendingTransactions()
        .build()
        .signAndAddSignature(key)
        .finish();
  }

  const std::string kNewRole = "rl"s;
  static const size_t kSignatories = 2;
  std::vector<crypto::Keypair> signatories;
};

/**
 * @given mst account, pair of signers and tx with a SetAccountDetail command
 * @when sending that tx with author signature @and then with signers' ones
 * @then commit appears only after tx is signed by all required signatories
 */
TEST_F(MstPipelineTest, OnePeerSendsTest) {
  auto tx = baseTx()
                .setAccountDetail(kUserId, "fav_meme", "doge")
                .quorum(kSignatories + 1);

  IntegrationTestFramework itf(1, {}, [](auto &i) { i.done(); }, true);
  itf.setInitialState(kAdminKeypair);
  auto &mst_itf = makeMstUser(itf);
  mst_itf
      .sendTx(signTx(tx, kUserKeypair))
      // TODO(@l4l) 21/05/18 IR-1339
      // tx should be checked for MST_AWAIT status
      .sendTx(signTx(tx, signatories[0]))
      .sendTx(signTx(tx, signatories[1]))
      .skipProposal()
      .skipVerifiedProposal()
      .checkBlock([](auto &proposal) {
        ASSERT_EQ(proposal->transactions().size(), 1);
      });
}

/**
 * @given ledger with pending transactions
 * @when get pending transactions is executed by peer, which is to sign those
 * transactions
 * @then those transactions is returned
 */
TEST_F(MstPipelineTest, DISABLED_GetPendingTxsAwaitingForThisPeer) {
  auto pending_tx = baseTx()
                        .setAccountDetail(kUserId, "fav_meme", "doge")
                        .quorum(kSignatories + 1);
  auto pending_tx_check = [](auto &response) {
    ASSERT_NO_THROW({
        //      const auto &pending_tx_resp = boost::apply_visitor(
        //          framework::SpecifiedVisitor<
        //              shared_model::interface::GetPendingTransactions>(),
        //          response.get());
        //      ASSERT_EQ(pending_tx_resp.getTxs[0].hash(), pending_tx.hash());
    });
  };

  // prepare itf
  IntegrationTestFramework itf(1, {}, [](auto &i) { i.done(); }, true);
  itf.setInitialState(kAdminKeypair);
  auto &mst_itf = makeMstUser(itf);

  // send pending transaction, signing it only with one signatory
  mst_itf.sendTx(signTx(pending_tx, signatories[0]))
      .sendQuery(makeGetPendingTxsQuery(kUserId, kUserKeypair),
                 pending_tx_check);
}

/**
 * @given ledger with pending transactions, which lack two or more signatures
 * @when signing those transactions with one signature @and executing get
 * pending transactions
 * @then they are returned with initial number of signatures plus one
 */
TEST_F(MstPipelineTest, DISABLED_GetPendingTxsLatestSignatures) {
  auto pending_tx = baseTx()
                        .setAccountDetail(kUserId, "fav_meme", "doge")
                        .quorum(kSignatories + 1);
  auto signatory_check = [](size_t expected_signatories_number) {
    return [](auto &response) {
      ASSERT_NO_THROW({
          //        const auto &pending_tx_resp = boost::apply_visitor(
          //            framework::SpecifiedVisitor<
          //                shared_model::interface::GetPendingTransactions>(),
          //            response.get());
          //      ASSERT_EQ(pending_tx_resp.getTxs[0].signatures().size,
          //      expected_signatories_number);
      });
    };
  };

  // prepare itf
  IntegrationTestFramework itf(1, {}, [](auto &i) { i.done(); }, true);
  itf.setInitialState(kAdminKeypair);
  auto &mst_itf = makeMstUser(itf);

  mst_itf.sendTx(signTx(pending_tx, signatories[0]))
      .sendQuery(makeGetPendingTxsQuery(kUserId, kUserKeypair),
                 signatory_check(1))
      .sendTx(signTx(pending_tx, signatories[1]))
      .sendQuery(makeGetPendingTxsQuery(kUserId, kUserKeypair),
                 signatory_check(2));
}

/**
 * @given ledger with pending transactions
 * @when signing them with number of signatures to get over quorum @and
 * executing get pending transactions
 * @then those transactions are not returned
 */
TEST_F(MstPipelineTest, DISABLED_GetPendingTxsNoSignedTxs) {
  auto pending_tx = baseTx()
                        .setAccountDetail(kUserId, "fav_meme", "doge")
                        .quorum(kSignatories + 1);
  auto no_txs_check = [](auto &response) {
    ASSERT_NO_THROW({
        //      const auto &pending_tx_resp = boost::apply_visitor(
        //          framework::SpecifiedVisitor<
        //              shared_model::interface::GetPendingTransactions>(),
        //          response.get());
        //      ASSERT_TRUE(pending_tx_resp.getTxs().empty);
    });
  };

  // prepare itf
  IntegrationTestFramework itf(1, {}, [](auto &i) { i.done(); }, true);
  itf.setInitialState(kAdminKeypair);
  auto &mst_itf = makeMstUser(itf);

  mst_itf.sendTx(signTx(pending_tx, signatories[0]))
      .sendTx(signTx(pending_tx, signatories[1]))
      .sendTx(signTx(pending_tx, kUserKeypair))
      .sendQuery(makeGetPendingTxsQuery(kUserId, kUserKeypair), no_txs_check);
}
