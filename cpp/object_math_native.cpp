#include "object_math_native.hpp"
#include "core/formula.h"
#include "latex.h"
#include "object_math_cairo.hpp"
#include "utils/utf.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <stdexcept>
namespace inde::ui {
void validate_native_math(const std::string &s) {
  if (s.empty() || s.size() > 16384 ||
      !g_utf8_validate(s.data(), static_cast<gssize>(s.size()), nullptr))
    throw std::runtime_error(
        "Confira a fonte da equação (até 16 KiB em UTF-8).");
  static const std::set<std::string> allowed = {"frac",
                                                "dfrac",
                                                "tfrac",
                                                "sqrt",
                                                "sum",
                                                "prod",
                                                "int",
                                                "iint",
                                                "iiint",
                                                "oint",
                                                "lim",
                                                "infty",
                                                "pm",
                                                "mp",
                                                "times",
                                                "cdot",
                                                "div",
                                                "le",
                                                "leq",
                                                "ge",
                                                "geq",
                                                "ne",
                                                "neq",
                                                "approx",
                                                "equiv",
                                                "sim",
                                                "simeq",
                                                "propto",
                                                "partial",
                                                "nabla",
                                                "alpha",
                                                "beta",
                                                "gamma",
                                                "delta",
                                                "epsilon",
                                                "varepsilon",
                                                "zeta",
                                                "eta",
                                                "theta",
                                                "vartheta",
                                                "iota",
                                                "kappa",
                                                "lambda",
                                                "mu",
                                                "nu",
                                                "xi",
                                                "pi",
                                                "varpi",
                                                "rho",
                                                "varrho",
                                                "sigma",
                                                "varsigma",
                                                "tau",
                                                "upsilon",
                                                "phi",
                                                "varphi",
                                                "chi",
                                                "psi",
                                                "omega",
                                                "Gamma",
                                                "Delta",
                                                "Theta",
                                                "Lambda",
                                                "Xi",
                                                "Pi",
                                                "Sigma",
                                                "Upsilon",
                                                "Phi",
                                                "Psi",
                                                "Omega",
                                                "sin",
                                                "cos",
                                                "tan",
                                                "cot",
                                                "sec",
                                                "csc",
                                                "arcsin",
                                                "arccos",
                                                "arctan",
                                                "sinh",
                                                "cosh",
                                                "tanh",
                                                "log",
                                                "ln",
                                                "exp",
                                                "min",
                                                "max",
                                                "det",
                                                "gcd",
                                                "left",
                                                "right",
                                                "big",
                                                "Big",
                                                "bigl",
                                                "bigr",
                                                "Bigl",
                                                "Bigr",
                                                "begin",
                                                "end",
                                                "text",
                                                "textrm",
                                                "mathrm",
                                                "mathbf",
                                                "mathit",
                                                "mathsf",
                                                "mathtt",
                                                "mathbb",
                                                "mathcal",
                                                "overline",
                                                "underline",
                                                "vec",
                                                "hat",
                                                "bar",
                                                "dot",
                                                "ddot",
                                                "tilde",
                                                "widehat",
                                                "widetilde",
                                                "overbrace",
                                                "underbrace",
                                                "binom",
                                                "dbinom",
                                                "limits",
                                                "nolimits",
                                                "displaystyle",
                                                "textstyle",
                                                "scriptstyle",
                                                "quad",
                                                "qquad",
                                                "ldots",
                                                "cdots",
                                                "vdots",
                                                "ddots",
                                                "to",
                                                "rightarrow",
                                                "leftarrow",
                                                "Rightarrow",
                                                "Leftarrow",
                                                "leftrightarrow",
                                                "Leftrightarrow",
                                                "mapsto",
                                                "in",
                                                "notin",
                                                "subset",
                                                "subseteq",
                                                "supset",
                                                "supseteq",
                                                "cup",
                                                "cap",
                                                "emptyset",
                                                "forall",
                                                "exists",
                                                "neg",
                                                "land",
                                                "lor",
                                                "wedge",
                                                "vee",
                                                "langle",
                                                "rangle",
                                                "lvert",
                                                "rvert",
                                                "lVert",
                                                "rVert",
                                                "vert",
                                                "Vert",
                                                "lfloor",
                                                "rfloor",
                                                "lceil",
                                                "rceil",
                                                "degree"};
  static const std::set<std::string> envs = {"matrix",  "pmatrix", "bmatrix",
                                             "Bmatrix", "vmatrix", "Vmatrix",
                                             "cases",   "aligned", "array"};
  int depth = 0;
  std::size_t tokens = 0, cells = 0;
  std::vector<std::string> environments;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (++tokens > 8192)
      throw std::runtime_error("A expressão excede o limite da prévia nativa.");
    if (s[i] == '{') {
      if (++depth > 32)
        throw std::runtime_error("A expressão tem níveis demais.");
    }
    if (s[i] == '}' && --depth < 0)
      throw std::runtime_error("Confira as chaves da expressão.");
    if (s[i] == '&' && ++cells > 256)
      throw std::runtime_error("A matriz excede o limite da prévia.");
    if (s[i] != '\\')
      continue;
    if (++i >= s.size())
      throw std::runtime_error("Complete o comando matemático.");
    if (!std::isalpha(static_cast<unsigned char>(s[i])))
      continue;
    const auto start = i;
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i])))
      ++i;
    const auto command = s.substr(start, i - start);
    if (!allowed.contains(command))
      throw std::runtime_error(
          "Comando ainda não disponível na prévia nativa: \\" + command);
    if (command == "begin" || command == "end") {
      while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
        ++i;
      if (i >= s.size() || s[i] != '{')
        throw std::runtime_error("Informe o tipo de matriz ou sistema.");
      const auto end = s.find('}', i);
      if (end == std::string::npos)
        throw std::runtime_error("Complete o ambiente matemático.");
      const auto name = s.substr(i + 1, end - i - 1);
      if (!envs.contains(name))
        throw std::runtime_error("Ambiente matemático não disponível: " + name);
      if (command == "begin") {
        if (environments.size() >= 8)
          throw std::runtime_error("Ambientes matemáticos aninhados demais.");
        environments.push_back(name);
      } else {
        if (environments.empty() || environments.back() != name)
          throw std::runtime_error(
              "Confira o início e o fim do ambiente matemático.");
        environments.pop_back();
      }
      i = end;
    } else
      --i;
  }
  if (depth || !environments.empty())
    throw std::runtime_error("Complete as chaves ou o ambiente da expressão.");
}
struct MathPreview::Impl {
  std::unique_ptr<tex::TeXRender> render;
  std::string error;
};
MathPreview::MathPreview() : impl_(std::make_unique<Impl>()) {
  set_content_width(240);
  set_content_height(80);
  set_hexpand(true);
  set_draw_func([this](const Cairo::RefPtr<Cairo::Context> &cr, int width,
                       int height) {
    if (impl_->render) {
      cr->save();
      GdkRGBA color{};
#if GTK_CHECK_VERSION(4, 10, 0)
      gtk_widget_get_color(GTK_WIDGET(gobj()), &color);
#else
      gtk_style_context_get_color(
          gtk_widget_get_style_context(GTK_WIDGET(gobj())), &color);
#endif
      const auto channel = [](float value) {
        return static_cast<tex::color>(std::clamp(value, 0.f, 1.f) * 255.f);
      };
      impl_->render->setForeground(
          (channel(color.alpha) << 24) | (channel(color.red) << 16) |
          (channel(color.green) << 8) | channel(color.blue));
      const auto w = impl_->render->getWidth(), h = impl_->render->getHeight();
      const double scale =
          std::min({1.0, static_cast<double>(width - 16) / std::max(w, 1),
                    static_cast<double>(height - 12) / std::max(h, 1)});
      cr->translate(8, 6);
      cr->scale(std::max(.1, scale), std::max(.1, scale));
      tex::Graphics2D_cairo graphics(cr);
      impl_->render->draw(graphics, 0, 0);
      cr->restore();
    }
  });
}
MathPreview::~MathPreview() = default;
void MathPreview::set_source(const std::string &source, bool compact) {
  impl_->render.reset();
  impl_->error.clear();
  set_tooltip_text(source);
  gtk_accessible_update_property(GTK_ACCESSIBLE(gobj()),
                                 GTK_ACCESSIBLE_PROPERTY_LABEL, source.c_str(),
                                 -1);
  try {
    validate_native_math(source);
    static const bool initialized = []() {
      std::filesystem::path p = INDE_MATH_RESOURCE_DIR;
      std::error_code ec;
      auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
      auto installed = exe.parent_path() / "../share/inde/math";
      if (!ec &&
          std::filesystem::is_regular_file(installed / ".clatexmath-res_root"))
        p = installed;
      if (!std::filesystem::is_regular_file(p / ".clatexmath-res_root"))
        throw std::runtime_error("Recursos matemáticos não encontrados.");
      tex::LaTeX::init(p.string());
      return true;
    }();
    (void)initialized;
    tex::Formula formula(tex::utf82wide(source));
    tex::TeXRenderBuilder builder;
    impl_->render.reset(
        builder.setStyle(compact ? tex::TexStyle::text : tex::TexStyle::display)
            .setTextSize(compact ? 20.f : 26.f)
            .setWidth(tex::UnitType::pixel, 900, tex::Alignment::left)
            .setIsMaxWidth(true)
            .setLineSpace(tex::UnitType::pixel, 6)
            .setForeground(0xff243d38)
            .build(formula));
    if (!impl_->render || impl_->render->getWidth() > 10000 ||
        impl_->render->getHeight() > 4000) {
      impl_->render.reset();
      throw std::runtime_error("A expressão é grande demais para esta prévia.");
    }
    set_content_height(std::clamp(impl_->render->getHeight() + 16, 48, 480));
  } catch (const std::exception &e) {
    impl_->error = e.what();
    set_content_height(32);
  }
  queue_draw();
}
const std::string &MathPreview::error() const { return impl_->error; }
bool MathPreview::rendered() const { return bool(impl_->render); }
} // namespace inde::ui
