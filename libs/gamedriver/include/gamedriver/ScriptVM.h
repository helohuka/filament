
#ifndef __SCRIPT_ENV_H__
#define __SCRIPT_ENV_H__ 1
#include <memory>
#include <string>
#include <lua.h>
#include <luacode.h>

class ScriptVM {
public:
	//Inside apis ;


public:
	~ScriptVM();
	static ScriptVM& get();

	ScriptVM(const ScriptVM& rhs) = delete;
	ScriptVM(ScriptVM&& rhs) = delete;
	ScriptVM& operator=(const ScriptVM& rhs) = delete;
	ScriptVM& operator=(ScriptVM&& rhs) = delete;
	
	//
	void setup();
	void shutdown();


	//Inside apis
	const char* getLastError();

	bool loadString(std::string& code, const char* chunkname = "ScriptVM");
	bool loadString(const char* code, const char* chunkname = "ScriptVM");
	bool doString(std::string& code);
	bool doString(const char* code);
		
private:
	ScriptVM();

	std::unique_ptr< lua_State, void (*)(lua_State*) > mGlobalState;
	std::string mLastError;
};


#endif // __SCRIPT_H__
