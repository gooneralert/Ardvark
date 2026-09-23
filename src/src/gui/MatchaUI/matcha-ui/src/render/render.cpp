#include "render.h"

render_t::render_t() = default;
render_t::~render_t() = default;
void render_t::start_render() {}
void render_t::render_menu() {}
void render_t::render_visuals() {}
void render_t::end_render() {}
bool render_t::create_device() { return false; }
bool render_t::create_window() { return false; }
bool render_t::create_imgui() { return false; }
void render_t::destroy_imgui() {}
void render_t::destroy_device() {}
void render_t::destroy_window() {}
