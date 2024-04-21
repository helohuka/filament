#ifndef __LUAUVM_H__
#define __LUAUVM_H__ 1

/*!
 * \file LuauVM.h
 * \date 2023.08.20
 *
 * \author Helohuka
 * 
 * Contact: helohuka@outlook.com
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note
*/


#include "BaseLibs.h"

class LuauVM 
{
public:

    LuauVM();
    ~LuauVM();

public:

	//Inside apis
	const char* getLastError();

	bool loadString(std::string& code, const char* chunkname = "LuauVM");
	bool loadString(const char* code, const char* chunkname = "LuauVM");
	bool doString(std::string& code);
	bool doString(const char* code);
		
private:
	
	typedef std::unique_ptr<lua_State, void (*)(lua_State*) >	LuaStatePtr;
	
	LuaStatePtr			mL;
	std::string			mLastError;
};
#endif // __LUAUVM_H__
