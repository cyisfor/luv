#include <lua.h>
#include <uv.h>
#include <stdint.h>

uv_buf_t* buffer_new_fromstring(lua_State* L, const char* s, size_t size);
uv_buf_t* buffer_new(lua_State* L, size_t size);
int buffer_prepare(lua_State* L, size_t size, uv_buf_t* buf);
void buffer_get(lua_State* L, int index, uv_buf_t* dest);
int buffer_slice(lua_State* L);
