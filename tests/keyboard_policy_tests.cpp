#include "inde/ui/keyboard_commands.hpp"
#include <cassert>
#include <iostream>
using namespace inde::ui;
int main() {
  std::size_t cases{};
  for(guint key=GDK_KEY_a;key<=GDK_KEY_z;++key)for(unsigned mask=0;mask<64;++mask) {
    unsigned bits{};
    for(unsigned i=0;i<6;++i)if(mask&(1u<<i))bits|=std::vector<unsigned>{GDK_SHIFT_MASK,GDK_CONTROL_MASK,GDK_ALT_MASK,GDK_SUPER_MASK,GDK_HYPER_MASK,GDK_META_MASK}[i];
    auto expected=bits==GDK_CONTROL_MASK;
    assert(shortcut_matches(key,static_cast<Gdk::ModifierType>(bits),"<Control>"+std::string(1,static_cast<char>(key)))==expected);
    assert(shortcut_matches(gdk_keyval_to_upper(key),static_cast<Gdk::ModifierType>(bits|GDK_LOCK_MASK),"<Control>"+std::string(1,static_cast<char>(key)))==expected);
    cases+=2;
  }
  assert(shortcut_matches(GDK_KEY_KP_8,Gdk::ModifierType::CONTROL_MASK,"<Control>8"));
  assert(shortcut_matches(GDK_KEY_KP_Enter,Gdk::ModifierType::CONTROL_MASK,"<Control>Return"));
  assert(shortcut_matches(GDK_KEY_KP_Add,Gdk::ModifierType::CONTROL_MASK,"<Control>plus"));
  assert(shortcut_matches(GDK_KEY_equal,Gdk::ModifierType::CONTROL_MASK,"<Control>plus"));
  assert(!shortcut_matches(GDK_KEY_k,static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK|GDK_ALT_MASK),"<Control><Shift>k"));
  assert(!shortcut_matches(GDK_KEY_space,static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK|GDK_ALT_MASK),""));
  std::cout<<"PASS keyboard policy "<<cases+6<<" exact modifier, lock and keypad cases\n";
}
