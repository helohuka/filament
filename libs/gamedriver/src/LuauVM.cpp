
#include "gamedriver/LuauVM.h"

//-----------------------------------------------------------------------

struct GlobalOptions
{
    int optimizationLevel = 1;
    int debugLevel = 1;
} globalOptions;

static Luau::CompileOptions copts()
{
    Luau::CompileOptions result = {};
    result.optimizationLevel = globalOptions.optimizationLevel;
    result.debugLevel = globalOptions.debugLevel;
    //result.coverageLevel = coverageActive() ? 2 : 0;

    return result;
}

//-----------------------------------------------------------------------

#ifdef CALLGRIND
static int lua_callgrind(lua_State* L)
{
    const char* option = luaL_checkstring(L, 1);

    if (strcmp(option, "running") == 0)
    {
        int r = RUNNING_ON_VALGRIND;
        lua_pushboolean(L, r);
        return 1;
    }

    if (strcmp(option, "zero") == 0)
    {
        CALLGRIND_ZERO_STATS;
        return 0;
    }

    if (strcmp(option, "dump") == 0)
    {
        const char* name = luaL_checkstring(L, 2);

        CALLGRIND_DUMP_STATS_AT(name);
        return 0;
    }

    luaL_error(L, "callgrind must be called with one of 'running', 'zero', 'dump'");
}
#endif

static int finishrequire(lua_State* L)
{
    if (lua_isstring(L, -1))
        lua_error(L);

    return 1;
}

int lua_require(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    std::string chunkname = "=" + name;

    luaL_findtable(L, LUA_REGISTRYINDEX, "_MODULES", 1);

    // return the module from the cache
    lua_getfield(L, -1, name.c_str());
    if (!lua_isnil(L, -1))
    {
        // L stack: _MODULES result
        return finishrequire(L);
    }

    lua_pop(L, 1);

    std::optional<std::string> source = readFile(name + ".luau");
    if (!source)
    {
        source = readFile(name + ".lua"); // try .lua if .luau doesn't exist
        if (!source)
            luaL_argerrorL(L, 1, ("error loading " + name).c_str()); // if neither .luau nor .lua exist, we have an error
    }

    // module needs to run in a new thread, isolated from the rest
    // note: we create ML on main thread so that it doesn't inherit environment of L
    lua_State* GL = lua_mainthread(L);
    lua_State* ML = lua_newthread(GL);
    lua_xmove(GL, L, 1);

    // new thread needs to have the globals sandboxed
    luaL_sandboxthread(ML);

    // now we can compile & run module on the new thread
    std::string bytecode = Luau::compile(*source, copts());
    if (luau_load(ML, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) == 0)
    {
        /*if (codegen)
     = std::move(std::unique_ptr< lua_State, void (*)(lua_State*) >();
            Luau::CodeGen::compile(ML, -1);

        if (coverageActive())
            coverageTrack(ML, -1);*/

        int status = lua_resume(ML, L, 0);

        if (status == 0)
        {
            if (lua_gettop(ML) == 0)
                lua_pushstring(ML, "module must return a value");
            else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1))
                lua_pushstring(ML, "module must return a table or function");
        }
        else if (status == LUA_YIELD)
        {
            lua_pushstring(ML, "module can not yield");
        }
        else if (!lua_isstring(ML, -1))
        {
            lua_pushstring(ML, "unknown error while running module");
        }
    }

    // there's now a return value on top of ML; L stack: _MODULES ML
    lua_xmove(ML, L, 1);
    lua_pushvalue(L, -1);
    lua_setfield(L, -4, name.c_str());

    // L stack: _MODULES ML result
    return finishrequire(L);
}

int lua_loadstring(lua_State* L)
{
    size_t l = 0;
    const char* s = luaL_checklstring(L, 1, &l);
    const char* chunkname = luaL_optstring(L, 2, s);

    lua_setsafeenv(L, LUA_ENVIRONINDEX, false);

    std::string bytecode = Luau::compile(std::string(s, l), copts());
    if (luau_load(L, chunkname, bytecode.data(), bytecode.size(), 0) == 0)
        return 1;

    lua_pushnil(L);
    lua_insert(L, -2); // put before error message
    return 2;          // return nil plus error message
}


int lua_collectgarbage(lua_State* L)
{
    const char* option = luaL_optstring(L, 1, "collect");

    if (strcmp(option, "collect") == 0)
    {
        lua_gc(L, LUA_GCCOLLECT, 0);
        return 0;
    }

    if (strcmp(option, "count") == 0)
    {
        int c = lua_gc(L, LUA_GCCOUNT, 0);
        lua_pushnumber(L, c);
        return 1;
    }

    luaL_error(L, "collectgarbage must be called with 'count' or 'collect'");
}

int lua_print(lua_State* L)
{
    int n = lua_gettop(L);
    
    utils::slog.d << "Lua :";

    for (int i = 1; i < n; ++i)
    {
        utils::slog.d << " " << lua_tostring(L, i) ;
    }

    utils::slog.d << utils::io::endl;

    return 0;
}

int lua_log(lua_State* L)
{
    int n = lua_gettop(L);

    utils::slog.d << "Lua :";

    for (int i = 1; i < n; ++i)
    {
        utils::slog.d << " " << lua_tostring(L, i);
    }

    utils::slog.d << utils::io::endl;

    return 0;
}

//-----------------------------------------------------------------------

const char* LuauVM::getLastError()
{   
    return mLastError.empty() ? nullptr : mLastError.c_str();
}

bool LuauVM::loadString(std::string& source, const char* chunkname)
{
    lua_State* L = mL.get();

    std::string bytecode = Luau::compile(source, copts());
    if (luau_load(L, chunkname, bytecode.data(), bytecode.size(), 0) == 0)
        return true;

    mLastError = lua_tostring(L, -1);
    lua_pop(L, 1);
    return false;
}

bool LuauVM::loadString(const char* code, const char* chunkname)
{
    std::string source(code);
    return loadString(source, chunkname);
}

bool LuauVM::doString(std::string& source)
{
    lua_State* L = mL.get();

    std::string bytecode = Luau::compile(source, copts());
    
    if (luau_load(L, "=stdin", bytecode.data(), bytecode.size(), 0) != 0)
    {
        mLastError = lua_tostring(L, -1);
        lua_pop(L, 1);
        return false;
    }

    lua_State* T = lua_newthread(L);

    lua_pushvalue(L, -2);
    lua_remove(L, -3);
    lua_xmove(L, T, 1);

    int status = lua_resume(T, NULL, 0);

    if (status == 0)
    {
        int n = lua_gettop(T);

        if (n)
        {
            luaL_checkstack(T, LUA_MINSTACK, "too many results to print");
            lua_getglobal(T, "_PRETTYPRINT");
            // If _PRETTYPRINT is nil, then use the standard print function instead
            if (lua_isnil(T, -1))
            {
                lua_pop(T, 1);
                lua_getglobal(T, "print");
            }
            lua_insert(T, 1);
            lua_pcall(T, n, 0, 0);
        }
        
        lua_pop(L, 1);

    }
    else
    {

        if (status == LUA_YIELD)
        {
            mLastError = "thread yielded unexpectedly";
        }
        else if (const char* str = lua_tostring(T, -1))
        {
            mLastError = str;
        }

        mLastError += "\nstack backtrace:\n";
        mLastError += lua_debugtrace(T);
        
        lua_pop(L, 1);
        return false;
    }

    return true;
}

bool LuauVM::doString(const char* code)
{
    std::string source(code);
    return doString(source);
}

//-----------------------------------------------------------------------


LuauVM::~LuauVM()
{

}

LuauVM::LuauVM()
    :mL(luaL_newstate(), lua_close)
{  
    lua_State* L = mL.get();
    luaL_openlibs(L);

    static const luaL_Reg funcs[] = {
    {"loadstring", lua_loadstring},
    {"require", lua_require},
    {"print", lua_print},
    {"log", lua_log},
    {"collectgarbage", lua_collectgarbage},
#ifdef CALLGRIND
        {"callgrind", lua_callgrind},
#endif
        {NULL, NULL},
    };

    lua_pushvalue(L, LUA_GLOBALSINDEX);
    luaL_register(L, NULL, funcs);
    lua_pop(L, 1);

    luaL_sandbox(L);
}


