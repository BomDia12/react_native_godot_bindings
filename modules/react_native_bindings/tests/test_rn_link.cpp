// Tests in .cpp files are only linked when something references them, because
// the module is a static library.
void rn_force_link_service_tests();
void rn_force_link_react_host_tests();
void rn_force_link_component_view_tests();
void rn_force_link_value_codec_tests();

void rn_force_link_tests() {
	rn_force_link_service_tests();
	rn_force_link_react_host_tests();
	rn_force_link_component_view_tests();
	rn_force_link_value_codec_tests();
}
