#include "inde/persistence/enchant_spelling_provider.hpp"

#include <algorithm>
#include <dlfcn.h>
#include <stdexcept>
#include <utility>

namespace inde::persistence {

struct EnchantSpellingProvider::Impl {
  using BrokerInit = void *(*)();
  using BrokerFree = void (*)(void *);
  using RequestDict = void *(*)(void *, const char *);
  using FreeDict = void (*)(void *, void *);
  using Check = int (*)(void *, const char *, std::ptrdiff_t);
  using Suggest = char **(*)(void *, const char *, std::ptrdiff_t,
                             std::size_t *);
  using FreeSuggestions = void (*)(void *, char **);

  void *library{};
  void *broker{};
  void *dictionary{};
  BrokerFree broker_free{};
  FreeDict free_dict{};
  Check check{};
  Suggest suggest{};
  FreeSuggestions free_suggestions{};
  std::string language;

  template <typename Function> Function symbol(const char *name) {
    return reinterpret_cast<Function>(dlsym(library, name));
  }

  explicit Impl(std::string requested_language)
      : language(std::move(requested_language)) {
    library = dlopen("libenchant-2.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!library)
      return;
    const auto broker_init = symbol<BrokerInit>("enchant_broker_init");
    broker_free = symbol<BrokerFree>("enchant_broker_free");
    const auto request_dict =
        symbol<RequestDict>("enchant_broker_request_dict");
    free_dict = symbol<FreeDict>("enchant_broker_free_dict");
    check = symbol<Check>("enchant_dict_check");
    suggest = symbol<Suggest>("enchant_dict_suggest");
    free_suggestions = symbol<FreeSuggestions>("enchant_dict_free_string_list");
    if (!broker_init || !broker_free || !request_dict || !free_dict || !check ||
        !suggest || !free_suggestions)
      return;
    broker = broker_init();
    if (broker)
      dictionary = request_dict(broker, language.c_str());
  }

  ~Impl() {
    if (dictionary && broker && free_dict)
      free_dict(broker, dictionary);
    if (broker && broker_free)
      broker_free(broker);
    if (library)
      dlclose(library);
  }
};

EnchantSpellingProvider::EnchantSpellingProvider(std::string language)
    : impl_(std::make_unique<Impl>(std::move(language))) {}

EnchantSpellingProvider::~EnchantSpellingProvider() = default;

bool EnchantSpellingProvider::available() const noexcept {
  return impl_ && impl_->dictionary && impl_->check;
}

std::string EnchantSpellingProvider::description() const {
  if (available())
    return "Português (Brasil) — Hunspell via Enchant";
  return "Dicionário ortográfico pt-BR indisponível; regras gramaticais "
         "continuam ativas";
}

bool EnchantSpellingProvider::check(const std::string &word) const {
  if (!available())
    return true;
  return impl_->check(impl_->dictionary, word.data(),
                      static_cast<std::ptrdiff_t>(word.size())) == 0;
}

std::vector<std::string>
EnchantSpellingProvider::suggest(const std::string &word,
                                 std::size_t limit) const {
  if (!available() || limit == 0)
    return {};
  std::size_t count{};
  char **items =
      impl_->suggest(impl_->dictionary, word.data(),
                     static_cast<std::ptrdiff_t>(word.size()), &count);
  std::vector<std::string> result;
  const auto take = std::min(count, limit);
  result.reserve(take);
  for (std::size_t index = 0; index < take; ++index)
    result.emplace_back(items[index]);
  if (items)
    impl_->free_suggestions(impl_->dictionary, items);
  return result;
}

} // namespace inde::persistence
