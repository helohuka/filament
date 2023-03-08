
#ifndef __SCRIPT_ENV_H__
#define __SCRIPT_ENV_H__ 1
#include <memory>
#include <string>
#include <lua.h>
#include <luacode.h>

class LuauVM {
public:
	//Inside apis ;


public:
	~LuauVM();
	static LuauVM& get();

	LuauVM(const LuauVM& rhs) = delete;
	LuauVM(LuauVM&& rhs) = delete;
	LuauVM& operator=(const LuauVM& rhs) = delete;
	LuauVM& operator=(LuauVM&& rhs) = delete;

	//Inside apis
	const char* getLastError();

	bool loadString(std::string& code, const char* chunkname = "LuauVM");
	bool loadString(const char* code, const char* chunkname = "LuauVM");
	bool doString(std::string& code);
	bool doString(const char* code);
		
private:
	LuauVM();
	
	typedef std::unique_ptr<lua_State, void (*)(lua_State*) >	LuaStatePtr;
	
	LuaStatePtr			mL;
	std::string			mLastError;
};


class LuauManager
{
	

};


#endif // __SCRIPT_H__
