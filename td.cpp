#include "td.hpp"

// overloaded magic
namespace detail {
template <class... Fs>
struct overload;

template <class F>
struct overload<F> : public F {
  explicit overload(F f) : F(f) {
  }
};
template <class F, class... Fs>
struct overload<F, Fs...>
    : public overload<F>
    , overload<Fs...> {
  overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
  }
  using overload<F>::operator();
  using overload<Fs...>::operator();
};
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
  return detail::overload<F...>(f...);
}

  Td::Td() {
    td::Log::set_verbosity_level(1);
    client_ = std::make_unique<td::Client>();
  }

  void Td::auth() {
    while(true) {
      if(are_authorized_) {
	return;
      } else {
        process_response(client_->receive(10));
      }
    }      
  }

  void Td::send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
    auto query_id = next_query_id();
    if (handler) {
      handlers_.emplace(query_id, std::move(handler));
    }
    client_->send({query_id, std::move(f)});
  }


  void Td::process_response(td::Client::Response response) {
    if (!response.object) {
      return;
    }
    //std::cerr << response.id << " " << to_string(response.object) << std::endl;
    if (response.id == 0) {
      return process_update(std::move(response.object));
    }
    auto it = handlers_.find(response.id);
    if (it != handlers_.end()) {
      it->second(std::move(response.object));
    }
  }

  void Td::process_update(td_api::object_ptr<td_api::Object> update) {
    td_api::downcast_call(
        *update, overloaded(
                     [this](td_api::updateAuthorizationState &update_authorization_state) {
                       authorization_state_ = std::move(update_authorization_state.authorization_state_);
                       on_authorization_state_update();
                     },
                     [](auto &update) {}));
  }

  void Td::on_authorization_state_update() {
    authentication_query_id_++;
    td_api::downcast_call(
        *authorization_state_,
        overloaded(
            [this](td_api::authorizationStateReady &) {
              are_authorized_ = true;
              std::cerr << "Got authorization" << std::endl;
            },
            [this](td_api::authorizationStateLoggingOut &) {
              are_authorized_ = false;
              std::cerr << "Logging out" << std::endl;
            },
            [this](td_api::authorizationStateClosing &) { std::cerr << "Closing" << std::endl; },
            [this](td_api::authorizationStateClosed &) {
              are_authorized_ = false;
              need_restart_ = true;
              std::cerr << "Terminated" << std::endl;
            },
            [this](td_api::authorizationStateWaitCode &wait_code) {
              std::string first_name;
              std::string last_name;
              if (!wait_code.is_registered_) {
                std::cerr << "Enter your first name: ";
                std::cin >> first_name;
                std::cerr << "Enter your last name: ";
                std::cin >> last_name;
              }
              std::cerr << "Enter authentication code: ";
              std::string code;
              std::cin >> code;
              send_query(td_api::make_object<td_api::checkAuthenticationCode>(code, first_name, last_name),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitPassword &) {
              std::cerr << "Enter authentication password: ";
              std::string password;
              std::cin >> password;
              send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitPhoneNumber &) {
              std::cerr << "Enter phone number: ";
              std::string phone_number;
              std::cin >> phone_number;
              send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(
                             phone_number, false /*allow_flash_calls*/, false /*is_current_phone_number*/),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitEncryptionKey &) {
              std::cerr << "Enter encryption key or DESTROY: ";
              std::string key;
              std::getline(std::cin, key);
              if (key == "DESTROY") {
                send_query(td_api::make_object<td_api::destroy>(), create_authentication_query_handler());
              } else {
                send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(std::move(key)),
                           create_authentication_query_handler());
              }
            },
            [this](td_api::authorizationStateWaitTdlibParameters &) {
              auto parameters = td_api::make_object<td_api::tdlibParameters>();
              parameters->use_message_database_ = true;
              parameters->use_secret_chats_ = true;
              parameters->api_id_ = 94575;
              parameters->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
              parameters->system_language_code_ = "en";
              parameters->device_model_ = "Desktop";
              parameters->system_version_ = "Unknown";
              parameters->application_version_ = "1.0";
              parameters->enable_storage_optimizer_ = true;
              send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)),
                         create_authentication_query_handler());
            }));
  }


  void Td::check_authentication_error(Object object) {
    if (object->get_id() == td_api::error::ID) {
      auto error = td::move_tl_object_as<td_api::error>(object);
      std::cerr << "Error: " << to_string(error);
      on_authorization_state_update();
    }
  }

  std::uint64_t Td::next_query_id() {
    return ++current_query_id_;
  }
