#include "buffer.h"
#include <lauxlib.h>
#include <string.h>

uv_buf_t* buffer_new_fromstring(lua_State* L, const char* s, size_t size) {
    uv_buf_t* ret = buffer_new(L, size);
    memcpy(ret->base,s,size);
    return ret;
}

uv_buf_t* buffer_new(lua_State* L, size_t size) {
    uv_buf_t* base = lua_newuserdata(L, size + sizeof(uv_buf_t));
    base->base = (void*)base + sizeof(uv_buf_t);
    base->len = size;
    lua_pushstring(L, "buffer.meta");
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_setmetatable(L, -2);
    return base;
}

int buffer_prepare(lua_State* L, size_t size, uv_buf_t* buf) {
    uv_buf_t* base = buffer_new(L, size);
    buf->base = base->base;
    buf->len = base->len;
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

void buffer_get(lua_State* L, int index, uv_buf_t* dest) {
    if(lua_isstring(L, index)) {
        size_t len;
        const char* s = lua_tolstring(L, index, &len);
        uv_buf_t* new = buffer_new_fromstring(L, s, len);
        dest->base = new->base;
        dest->len = new->len;
        return;
    }
    lua_pushstring(L, "slice.meta");
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_getmetatable(L, index);
    int eq = (1 == lua_equal(L, -1, -2));
    lua_pop(L, 2);
    if(eq) {
        lua_getfield(L, -1, "start");
        int start = lua_tointeger(L, -1);
        lua_getfield(L, -2, "length");
        int length = lua_tointeger(L, -1);
        lua_getfield(L, -3, "raw");
        uv_buf_t* buf = lua_touserdata(L, -1);
        dest->base = buf->base + start;
        dest->len = length;
    } else {
        if(!lua_isuserdata(L, index)) {
            luaL_error(L, "not getting a buffer?");
        }
        uv_buf_t* buf = lua_touserdata(L, index);
        dest->base = buf->base;
        dest->len = buf->len;
    }
}

static int lbuffer_new(lua_State* L) {
    int size = lua_tointeger(L, 1);
    buffer_new(L, size);
    return 1;
}

/* A slice is basically a table with 3 fields, start, end, and the raw buffer */

int buffer_slice(lua_State* L) {
    int start = luaL_checkint(L, 2);
    int end = luaL_checkint(L, 3);
    int length, realstart;
    
    lua_pushstring(L, "slice.meta");
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_getmetatable(L, 1);
    int eq = (1 == lua_equal(L, -1, -2));
    lua_pop(L, 1); // save the slice metatable for later if we need create a new slice.

    if(eq) {
        lua_getfield(L, 1, "start");
        realstart = lua_tointeger(L, -1);
        lua_getfield(L, 1, "length");
        length = lua_tointeger(L, -1);
        lua_pop(L, 2);
    } else {
        if(!lua_isuserdata(L, 1))
            return luaL_error(L, "First argument does not appear to be a buffer!");
        uv_buf_t* buf = lua_touserdata(L, 1);
        realstart = 0;
        length = buf->len;
    }

    if(start < 0) {
        start = length + start;
        if(start < 0) 
            return luaL_error(L, "start index too low");
    }
    if(end < 0) {
        end = length + end;
        if(end < 0)
            return luaL_error(L, "end index too low");
    }
    if(end < start)
        return luaL_error(L, "end before start");
    if(end >= length)
        return luaL_error(L, "end index too high");
    // already checked that start < end
    if(start == 0 && end == length - 1) {
        lua_pop(L, 1); // slice metatable
        // no difference, can just return the slice or buffer passed.
        lua_pushvalue(L, 1);
        return 1;
    }

    lua_createtable(L, 0, 3);
    lua_insert(L, -2); // swap metatable and table
    lua_setmetatable(L, -2);

    lua_pushcfunction(L, buffer_slice);
    lua_setfield(L, -2, "slice");

    if(eq) {
        lua_getfield(L, 1, "raw");
    } else {
        lua_pushvalue(L, 1);
    }
    lua_setfield(L, -2, "raw");
    lua_pushinteger(L, realstart + start);
    lua_setfield(L, -2, "start");
    lua_pushinteger(L, end - start);
    lua_setfield(L, -2, "length");
    return 1;
}

static int buffer_index(lua_State* L) {
    if(lua_isnumber(L, 2)) {
        uv_buf_t* buf = lua_touserdata(L, 1);
        int index = lua_tointeger(L, 2);
        if(index < 0 || index > buf->len) {
            luaL_error(L, "Buffer overrun %d not betw 0 and %d",index,buf->len);
        }
        lua_pushinteger(L, buf->base[index]);
        return 1;
    }
    size_t slen;
    const char* name = lua_tolstring(L, 2, &slen);
    lua_remove(L, 2); // now stack is: buf start end (for slice)
#define EQUALS(_name) 0 == memcmp(name,_name, slen < sizeof(name) ? slen : sizeof(name))
    /* userdata can't have indexes so just need to put the direct (non-meta) methods here */
    if(EQUALS("slice")) {
        return buffer_slice(L);
    } else if(EQUALS("decode")) {
        // derp should actually decode like with utf-8 or somethin
        uv_buf_t* buf = lua_touserdata(L, 1);
        lua_pushlstring(L, buf->base, buf->len);
    }
#undef EQUALS
    return 1;
}

static int buffer_tostring(lua_State* L) {
    uv_buf_t* buf = lua_touserdata(L, 1);
    if(buf == NULL) 
        luaL_error(L, "Not a buffer we're trying to string.");
    lua_pushfstring(L, "<buffer %p:%d>",buf,buf->len);
    return 1;
}

static int slice_index(lua_State* L) {
    if(lua_isnumber(L, 2)) {
        int index = lua_tointeger(L, 2);
        if(index < 0) 
            return luaL_error(L, "index too low");
        lua_getfield(L, 1, "length");
        int length = lua_tointeger(L, -1);
        lua_pop(L,1);
        if(index >= length)
            return luaL_error(L, "index too high %d >= %d",index,length);
        lua_getfield(L, 1, "start");
        int realstart = lua_tointeger(L, -1);
        lua_pop(L,1);
        lua_getfield(L, 1, "raw");
        uv_buf_t* buf = lua_touserdata(L, -1);
        lua_pop(L, 1);
        lua_pushinteger(L, buf->base[realstart + index]);
        return 1;
    }
    size_t slen = 0;
    const char* name = lua_tolstring(L, 2, &slen);
#define EQUALS(_name) 0 == memcmp(name,_name, slen < sizeof(name) ? slen : sizeof(name))
    if(EQUALS("encode")) {
        lua_getfield(L, -1, "start");
        int start = lua_tointeger(L, -1);
        lua_getfield(L, -2, "length");
        int length = lua_tointeger(L, -1);
        lua_getfield(L, -3, "raw");
        uv_buf_t* buf = lua_touserdata(L, -1);
        lua_pop(L, 3);
        lua_pushlstring(L, buf->base + start, length);
        return 1;
    } else {
        return luaL_error(L, "No property called %s",name);
    }
#undef EQUALS
}

static int slice_tostring(lua_State* L) {
    lua_getfield(L, -1, "start");
    int start = lua_tointeger(L, -1);
    lua_getfield(L, -2, "length");
    int length = lua_tointeger(L, -1);
    lua_getfield(L, -3, "raw");
    uv_buf_t* buf = lua_touserdata(L, -1);
    lua_pop(L, 3);

    lua_pushfstring(L, "<slice %d:%d <buffer %p:%d>>",start,length,buf,buf->len);
    return 1;
}

luaL_Reg bufferMeta[] = {
    { "__index", buffer_index },
    { "__tostring", buffer_tostring }
};

luaL_Reg sliceMeta[] = {
    { "__index", slice_index },
    { "__tostring", slice_tostring }
};

void buffer_init(lua_State* L) {
    lua_createtable(L, 0, 1);
    luaL_register(L, NULL, bufferMeta);
    lua_pushstring(L, "buffer.meta");
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_createtable(L, 0, 1);
    luaL_register(L, NULL, sliceMeta);
    lua_pushstring(L, "slice.meta");
    lua_rawset(L, LUA_REGISTRYINDEX);
}

int luaopen__buffer(lua_State* L) {
    buffer_init(L);
    lua_pushcfunction(L, lbuffer_new);
    return 1;
}
