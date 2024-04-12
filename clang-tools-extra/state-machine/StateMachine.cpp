#include "StateMachine.h"

struct Coin {};
struct Push {};

void F(std::unique_ptr<std::string> message) {
  if (message) {
    std::cout << *message << '\n';
  }
}

// https://en.wikipedia.org/wiki/Finite-state_machine#/media/File:Turnstile_state_machine_colored.svg
class Turnstile : public StateMachine<Turnstile> {
 public:
  Turnstile() : StateMachine(&Turnstile::Locked) {
    std::cout << "==> Locked\n";
  }

 private:
  void Locked() {
    state([](auto* message) {
      assert(message);
      std::cout << "Unhandled " << message->GetTypeId().Name()
                << " message in Locked.\n";
    })
        .on<Coin>([&] /*(Coin)*/ {
          state_ = &Turnstile::Unlocked;
          std::cout << "==> Unlocked\n";
        })
        .on<Push>([](Push) { std::cout << "==> Locked\n"; });
  }

  void Unlocked() {
    state()
        .on<Coin>([](const Coin&) { std::cout << "==> Unlocked\n"; })
        .on<Push>([&](Push&&) {
          state_ = &Turnstile::Locked;
          std::cout << "==> Locked\n";
        });
  }
};

// cd ~/git/llvm-project/build
// ninja
// bin/state-machine
int main() {
  Turnstile turnstile;
  for (int ch; (ch = std::getchar()) != EOF;) {
    if (ch == '\n') {
      continue;
    }

    if (ch == 'c') {
      std::cout << "On Coin...\n";
      turnstile.Send(Coin());
    } else if (ch == 'p') {
      std::cout << "On Push...\n";
      turnstile.Send(Push());
    } else if (ch == 'u') {
      std::cout << "On std::unique_ptr<std::string>...\n";
      turnstile.Send(
          std::make_unique<std::string>("move-only message example"));
    } else {
      turnstile.Send(42);
      turnstile.Send("42");
    }
  }
}
