#include <cassert>
#include <concepts>
#include <iostream>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace internal {
template <typename T>
constexpr auto TypeNameFor() {
  std::string_view name, prefix, suffix;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix = "auto internal::TypeNameFor() [T = ";
  suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix = "constexpr auto internal::TypeNameFor() [with T = ";
  suffix = "]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix = "auto __cdecl internal::TypeNameFor<";
  suffix = ">(void)";
#endif
  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
}

class TypeId final {
 public:
  bool operator==(TypeId other) const { return id_ == other.id_; }

  template <typename T>
  static TypeId For() {
    return TypeId(&For<T>, TypeNameFor<T>());
  }

  std::string_view Name() const { return name_; }

 private:
  using Id = TypeId (*)();

  explicit TypeId(Id id, std::string_view name) : id_(id), name_(name) {}

  Id id_;
  std::string_view name_;
};

class MessageBase {
 public:
  virtual TypeId GetTypeId() const = 0;

 protected:
  /* non-virtual */ ~MessageBase() = default;
};

template <typename Message>
class MessageWrapper final : public MessageBase {
 public:
  template <typename M>
  explicit MessageWrapper(M&& message) : message_(std::forward<M>(message)) {}

  Message&& operator*() && { return std::move(message_); }

 private:
  TypeId GetTypeId() const override { return TypeId::For<Message>(); }

  Message message_;
};

template <typename M, typename D>
concept Message =                           // A Message is something that...
    !std::same_as<M, void>                  // ...is not void...
    && std::same_as<M, std::decay_t<M>>     // ...is already a decayed type...
    && !D::template IsAlreadyHandled<M>();  // ...hasn't been handled by any
                                            // previous dispatchers.

template <typename H, typename M>
concept Handler =          // A Handler is something that is invocable...
    std::invocable<H, M>   // ...either with M...
    || std::invocable<H>;  // ...or without arguments.

template <typename Handler,
          typename Message = void,
          typename PreviousDispatcher = void>
class Dispatcher {
  static_assert(std::is_void_v<Message> == std::is_void_v<PreviousDispatcher>);

  static constexpr bool kIsSink = std::is_void_v<Message>;

  template <typename M>
  static constexpr bool IsAlreadyHandled() {
    if constexpr (!kIsSink) {
      return std::is_same_v<M, Message> ||
             PreviousDispatcher::template IsAlreadyHandled<M>();
    }

    return false;
  }

 public:
  template <typename H>
  Dispatcher(MessageBase* message, H&& handler)
    requires(kIsSink)
      : message_(message), handler_(std::forward<H>(handler)) {}

  template <typename H>
  Dispatcher(MessageBase* message,
             H&& handler,
             const PreviousDispatcher* previous_dispatcher)
    requires(!kIsSink)
      : message_(message),
        handler_(std::forward<H>(handler)),
        previous_dispatcher_(previous_dispatcher) {}

  ~Dispatcher() noexcept(false) {
    if (message_) {
      Dispatch(std::exchange(message_, nullptr));
    }
  }

  template <internal::Message<Dispatcher> M, internal::Handler<M&&> H>
  auto on(H&& handler) {
    return Dispatcher<std::decay_t<H>, M, Dispatcher>(
        std::exchange(message_, nullptr), std::forward<H>(handler), this);
  }

 private:
  template <typename, typename, typename>
  friend class Dispatcher;

  void Dispatch(MessageBase* message) const {
    if constexpr (!kIsSink) {
      if (message->GetTypeId() != TypeId::For<Message>()) {
        return previous_dispatcher_->Dispatch(message);
      }

      if constexpr (std::invocable<Handler>) {
        handler_();
      } else {
        handler_(*std::move(static_cast<MessageWrapper<Message>&>(*message)));
      }
    } else {
      handler_(message);
    }
  }

  MessageBase* message_;
  Handler handler_;
  struct Empty {};
  [[no_unique_address]] std::
      conditional_t<kIsSink, Empty, const PreviousDispatcher*>
          previous_dispatcher_;
};
}  // namespace internal

template <typename T>
class StateMachine {
 public:
  template <typename Message>
  void Send(Message&& message) {
    auto message_wrapper = internal::MessageWrapper<std::decay_t<Message>>(
        std::forward<Message>(message));
    message_ = &message_wrapper;
    (static_cast<T*>(this)->*state_)();
    assert(!message_);
  }

 protected:
  using State = void (T::*)();

  explicit StateMachine(State state) : state_(state) {}

  template <typename H>
  auto state(H&& handler) {
    return internal::Dispatcher<std::decay_t<H>>(
        std::exchange(message_, nullptr), std::forward<H>(handler));
  }

  auto state() {
    return state([](auto* message) {
      assert(message);
      std::cout << "Unhandled " << message->GetTypeId().Name() << " message.\n";
    });
  }

  State state_;

 private:
  internal::MessageBase* message_;
};
