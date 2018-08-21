/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ametsuchi/impl/postgres_query_executor.hpp"

#include <boost-tuple.h>
#include <soci/boost-tuple.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#include "ametsuchi/impl/soci_utils.hpp"
#include "interfaces/queries/blocks_query.hpp"

using namespace shared_model::interface::permissions;

namespace {
  /**
   * Generates a query response that contains an error response
   * @tparam T The error to return
   * @param query_hash Query hash
   * @return smart pointer with the QueryResponse
   */
  template <class T>
  shared_model::proto::TemplateQueryResponseBuilder<1> buildError() {
    return shared_model::proto::TemplateQueryResponseBuilder<0>()
        .errorQueryResponse<T>();
  }

  /**
   * Generates a query response that contains a concrete error (StatefulFailed)
   * @param query_hash Query hash
   * @return smart pointer with the QueryResponse
   */
  shared_model::proto::TemplateQueryResponseBuilder<1> statefulFailed() {
    return buildError<shared_model::interface::StatefulFailedErrorResponse>();
  }

  /**
   * Transforms result to optional
   * value -> optional<value>
   * error -> nullopt
   * @tparam T type of object inside
   * @param result BuilderResult
   * @return optional<T>
   */
  template <typename T>
  boost::optional<std::shared_ptr<T>> fromResult(
      shared_model::interface::CommonObjectsFactory::FactoryResult<
          std::unique_ptr<T>> &&result) {
    return result.match(
        [](iroha::expected::Value<std::unique_ptr<T>> &v) {
          return boost::make_optional(std::shared_ptr<T>(std::move(v.value)));
        },
        [](iroha::expected::Error<std::string>)
            -> boost::optional<std::shared_ptr<T>> { return boost::none; });
  }

  std::string getDomainFromName(const std::string &account_id) {
    std::vector<std::string> res;
    boost::split(res, account_id, boost::is_any_of("@"));
    return res.size() > 1 ? res.at(1) : "";
  }

  std::string checkAccountRolePermission(
      shared_model::interface::permissions::Role permission,
      const std::string &account_alias = "role_account_id") {
    const auto perm_str =
        shared_model::interface::RolePermissionSet({permission}).toBitstring();
    const auto bits = shared_model::interface::RolePermissionSet::size();
    std::string query = (boost::format(R"(
          SELECT COALESCE(bit_or(rp.permission), '0'::bit(%1%))
          & '%2%' = '%2%' FROM role_has_permissions AS rp
              JOIN account_has_roles AS ar on ar.role_id = rp.role_id
              WHERE ar.account_id = :%3%)")
                         % bits % perm_str % account_alias)
                            .str();
    return query;
  }

  auto hasQueryPermission(const std::string &creator,
                          const std::string &target_account,
                          Role indiv_permission_id,
                          Role all_permission_id,
                          Role domain_permission_id) {
    const auto bits = shared_model::interface::RolePermissionSet::size();
    const auto perm_str =
        shared_model::interface::RolePermissionSet({indiv_permission_id})
            .toBitstring();
    const auto all_perm_str =
        shared_model::interface::RolePermissionSet({all_permission_id})
            .toBitstring();
    const auto domain_perm_str =
        shared_model::interface::RolePermissionSet({domain_permission_id})
            .toBitstring();

    boost::format cmd(R"(
    WITH
        has_indiv_perm AS (
          SELECT COALESCE(bit_or(rp.permission), '0'::bit(%1%))
          & '%3%' = '%3%' FROM role_has_permissions AS rp
              JOIN account_has_roles AS ar on ar.role_id = rp.role_id
              WHERE ar.account_id = '%2%'
        ),
        has_all_perm AS (
          SELECT COALESCE(bit_or(rp.permission), '0'::bit(%1%))
          & '%4%' = '%4%' FROM role_has_permissions AS rp
              JOIN account_has_roles AS ar on ar.role_id = rp.role_id
              WHERE ar.account_id = '%2%'
        ),
        has_domain_perm AS (
          SELECT COALESCE(bit_or(rp.permission), '0'::bit(%1%))
          & '%5%' = '%5%' FROM role_has_permissions AS rp
              JOIN account_has_roles AS ar on ar.role_id = rp.role_id
              WHERE ar.account_id = '%2%'
        )
    SELECT ('%2%' = '%6%' AND (SELECT * FROM has_indiv_perm))
        OR (SELECT * FROM has_all_perm)
        OR ('%7%' = '%8%' AND (SELECT * FROM has_domain_perm)) AS perm
    )");

    return (cmd % bits % creator % perm_str % all_perm_str % domain_perm_str
            % target_account % getDomainFromName(creator)
            % getDomainFromName(target_account))
        .str();
  }
}  // namespace

using namespace shared_model::interface::permissions;

namespace iroha {
  namespace ametsuchi {
    PostgresQueryExecutor::PostgresQueryExecutor(
        std::shared_ptr<ametsuchi::Storage> storage,
        soci::session &sql,
        std::shared_ptr<shared_model::interface::CommonObjectsFactory> factory)
        : storage_(storage),
          sql_(sql),
          factory_(factory),
          visitor_(sql_, factory_) {}

    QueryExecutorResult PostgresQueryExecutor::validateAndExecute(
        const shared_model::interface::Query &query) {
      visitor_.setCreatorId(query.creatorAccountId());
      auto result = boost::apply_visitor(visitor_, query.get());
      return clone(result.queryHash(query.hash()).build());
    }

    bool PostgresQueryExecutor::validate(
        const shared_model::interface::BlocksQuery &query) {
      boost::format cmd(R"(%s)");
      soci::statement st =
          (sql_.prepare
           << (cmd % checkAccountRolePermission(Role::kGetBlocks)).str());
      int has_perm;
      st.exchange(soci::use(query.creatorAccountId(), "role_account_id"));
      st.exchange(soci::into(has_perm));
      st.define_and_bind();
      st.execute(true);

      return has_perm > 0;
    }

    PostgresQueryExecutorVisitor::PostgresQueryExecutorVisitor(
        soci::session &sql,
        std::shared_ptr<shared_model::interface::CommonObjectsFactory> factory)
        : sql_(sql), factory_(factory) {}

    void PostgresQueryExecutorVisitor::setCreatorId(
        const shared_model::interface::types::AccountIdType &creator_id) {
      creator_id_ = creator_id;
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccount &q) {
      using T = boost::tuple<boost::optional<std::string>,
                             boost::optional<std::string>,
                             boost::optional<int>,
                             boost::optional<std::string>,
                             boost::optional<std::string>,
                             int>;
      T tuple;
      auto cmd = (boost::format(R"(WITH has_perms AS (%s),
      t AS (
          SELECT a.account_id, a.domain_id, a.quorum, a.data, ARRAY_AGG(ar.role_id) AS roles
          FROM account AS a, account_has_roles AS ar
          WHERE a.account_id = :target_account_id
          AND ar.account_id = a.account_id
          GROUP BY a.account_id
      )
      SELECT account_id, domain_id, quorum, data, roles, perm
      FROM t RIGHT OUTER JOIN has_perms AS p ON TRUE
      )")
                  % hasQueryPermission(creator_id_,
                                       q.accountId(),
                                       Role::kGetMyAccount,
                                       Role::kGetAllAccounts,
                                       Role::kGetDomainAccounts))
                     .str();
      soci::statement st = (sql_.prepare << cmd);
      st.exchange(soci::use(q.accountId(), "target_account_id"));
      st.exchange(soci::into(tuple));

      st.define_and_bind();
      st.execute(true);

      if (not tuple.get<5>()) {
        return statefulFailed();
      }

      if (not tuple.get<0>().is_initialized()) {
        return buildError<shared_model::interface::NoAccountErrorResponse>();
      }

      auto roles_str = tuple.get<4>().get();
      roles_str.erase(0, 1);
      roles_str.erase(roles_str.size() - 1, 1);

      std::vector<shared_model::interface::types::RoleIdType> roles;

      boost::split(roles, roles_str, [](char c) { return c == ','; });

      auto account = fromResult(factory_->createAccount(q.accountId(),
                                                        tuple.get<1>().get(),
                                                        tuple.get<2>().get(),
                                                        tuple.get<3>().get()));

      if (not account) {
        // fix it
        return buildError<shared_model::interface::NoAccountErrorResponse>();
      }

      auto response = QueryResponseBuilder().accountResponse(
          *std::static_pointer_cast<shared_model::proto::Account>(*account),
          roles);
      return response;
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetSignatories &q) {
      std::vector<shared_model::interface::types::PubkeyType> pubkeys;
      soci::indicator ind;
      boost::tuple<boost::optional<std::string>, int> row;
      auto cmd = (boost::format(R"(WITH has_perms AS (%s),
      t AS (
          SELECT account_id FROM account_has_signatory
          WHERE account_id = :account_id
      )
      SELECT account_id, perm FROM t
      RIGHT OUTER JOIN has_perms ON TRUE
      )")
                  % hasQueryPermission(creator_id_,
                                       q.accountId(),
                                       Role::kGetMySignatories,
                                       Role::kGetAllSignatories,
                                       Role::kGetDomainSignatories))
                     .str();
      soci::statement st = (sql_.prepare << cmd);
      st.exchange(soci::use(q.accountId()));
      st.exchange(soci::into(row, ind));

      st.define_and_bind();
      st.execute();

      int has_perm = -1;

      processSoci(st,
                  ind,
                  row,
                  [&pubkeys, &has_perm](
                      boost::tuple<boost::optional<std::string>, int> &row) {
                    has_perm = row.get<1>();
                    if (row.get<0>()) {
                      pubkeys.push_back(shared_model::crypto::PublicKey(
                          shared_model::crypto::Blob::fromHexString(
                              row.get<0>().get())));
                    }
                  });

      if (not has_perm) {
        return statefulFailed();
      }

      if (pubkeys.empty()) {
        return buildError<
            shared_model::interface::NoSignatoriesErrorResponse>();
      }
      return QueryResponseBuilder().signatoriesResponse(pubkeys);
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccountTransactions &q) {
      return statefulFailed();
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetTransactions &q) {
      return statefulFailed();
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccountAssetTransactions &q) {
      return statefulFailed();
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccountAssets &q) {
      soci::indicator ind;
      using T = boost::tuple<boost::optional<std::string>,
                             boost::optional<std::string>,
                             boost::optional<std::string>,
                             int>;
      T row;
      auto cmd = (boost::format(R"(WITH has_perms AS (%s),
      t AS (
          SELECT * FROM account_has_asset
          WHERE account_id = :account_id
      )
      SELECT account_id, asset_id, amount, perm FROM t
      RIGHT OUTER JOIN has_perms ON TRUE
      )")
                  % hasQueryPermission(creator_id_,
                                       q.accountId(),
                                       Role::kGetMyAccAst,
                                       Role::kGetAllAccAst,
                                       Role::kGetDomainAccAst))
                     .str();
      soci::statement st = (sql_.prepare << cmd);
      st.exchange(soci::use(q.accountId()));
      st.exchange(soci::into(row, ind));

      st.define_and_bind();
      st.execute();

      int has_perm = -1;

      std::vector<shared_model::proto::AccountAsset> account_assets;
      std::vector<std::shared_ptr<shared_model::interface::AccountAsset>>
          assets;
      processSoci(st, ind, row, [&account_assets, &has_perm, this](T &row) {
        has_perm = row.get<3>();
        if (row.get<0>()) {
          fromResult(factory_->createAccountAsset(
              row.get<0>().get(),
              row.get<1>().get(),
              shared_model::interface::Amount(row.get<2>().get())))
              | [&account_assets](const auto &asset) {
                  account_assets.push_back(
                      *std::static_pointer_cast<
                          shared_model::proto::AccountAsset>(asset));
                };
        }
      });

      if (not has_perm) {
        return statefulFailed();
      }

      if (account_assets.empty()) {
        return buildError<
            shared_model::interface::NoAccountAssetsErrorResponse>();
      }
      return QueryResponseBuilder().accountAssetResponse(account_assets);
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccountDetail &q) {
      return statefulFailed();
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetRoles &q) {
      return statefulFailed();
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetRolePermissions &q) {
      return statefulFailed();
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAssetInfo &q) {
      return statefulFailed();
    }

    QueryResponseBuilderDone PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetPendingTransactions &q) {
      return statefulFailed();
    }
  }  // namespace ametsuchi
}  // namespace iroha
