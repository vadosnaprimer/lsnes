#include "core/keymapper.hpp"
#include "core/command.hpp"
#include "lua/internal.hpp"
#include <stdexcept>

namespace
{
	int kbd_bind(lua::state& L, lua::parameters& P)
	{
		std::string mod, mask, key, cmd;

		P(mod, mask, key, cmd);

		lsnes_mapper.bind(mod, mask, key, cmd);
		return 0;
	}

	int kbd_unbind(lua::state& L, lua::parameters& P)
	{
		std::string mod, mask, key;

		P(mod, mask, key);

		lsnes_mapper.unbind(mod, mask, key);
		return 0;
	}

	int kbd_alias(lua::state& L, lua::parameters& P)
	{
		std::string alias, cmds;

		P(alias, cmds);

		lsnes_cmd.set_alias_for(alias, cmds);
		refresh_alias_binds();
		return 0;
	}

	class lua_keyboard_dummy {};
	lua::_class<lua_keyboard_dummy> lua_kbd(lua_class_bind, "*keyboard", {
		{"bind", kbd_bind},
		{"unbind", kbd_unbind},
		{"alias", kbd_alias},
	});
}
