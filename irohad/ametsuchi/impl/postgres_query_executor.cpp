/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ametsuchi/impl/postgres_query_executor.hpp"

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
      return boost::apply_visitor(visitor_, query.get());
    }

    PostgresQueryExecutorVisitor::PostgresQueryExecutorVisitor(
        soci::session &sql,
        std::shared_ptr<shared_model::interface::CommonObjectsFactory> factory)
        : sql_(sql), factory_(factory) {}

    void PostgresQueryExecutorVisitor::setCreatorId(
        shared_model::interface::types::AccountIdType &creator_id) {
      creator_id_ = creator_id;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccount &q) {
      using T = boost::tuple<boost::optional<std::string>,
                             std::string,
                             int,
                             std::string,
                             std::string,
                             int>;
      T tuple;
      soci::statement st = (sql_.prepare <<
                            R"(WITH has_perm AS ()"
                                + hasQueryPermission(Role::kGetMyAccount,
                                                     Role::kGetAllAccounts,
                                                     Role::kGetDomainAccounts)
                                + R"(),
      SELECT a.account_id, a.domain_id, a.quorum, a.data, ARRAY_AGG(ar.role_id), p.perm
      FROM account AS a, has_perm AS p
      JOIN account_has_roles AS ar ON a.account_id = ar.account_id
      WHERE account_id = :target_account_id
      GROUP BY a.account_id
      ))");
      st.exchange(soci::use(creator_id_, "role_account_id"));
      st.exchange(soci::use(q.accountId(), "target_account_id"));
      st.exchange(soci::use(getDomainFromName(creator_id_), "creator_domain"));
      st.exchange(soci::use(getDomainFromName(q.accountId()), "target_domain"));
      st.exchange(soci::into(tuple));

      st.define_and_bind();
      st.execute(true);

      shared_model::interface::types::HashType hash;

      if (tuple.get<0>().is_initialized()) {
        return clone(
            buildError<shared_model::interface::NoAccountErrorResponse>()
                .queryHash(hash)
                .build());
      }

      if (tuple.get<5>()) {
        return clone(statefulFailed().queryHash(hash).build());
      }

      auto account = fromResult(factory_->createAccount(
          q.accountId(), tuple.get<1>(), tuple.get<2>(), tuple.get<3>()));

      auto roles =

          return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetSignatories &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccountTransactions &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetTransactions &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccountAssetTransactions &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccountAssets &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAccountDetail &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetRoles &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetRolePermissions &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetAssetInfo &q) {
      return nullptr;
    }

    QueryExecutorResult PostgresQueryExecutorVisitor::operator()(
        const shared_model::interface::GetPendingTransactions &q) {
      return nullptr;
    }
  }  // namespace ametsuchi
}  // namespace iroha
