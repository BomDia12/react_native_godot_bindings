// Tests in .cpp files are only linked when something references them, because
// the module is a static library.
void rn_force_link_service_tests();
void rn_force_link_react_host_tests();
void rn_force_link_component_view_tests();
void rn_force_link_value_codec_tests();
void rn_force_link_style_tests();
void rn_force_link_text_tests();
void rn_force_link_accessibility_tests();
void rn_force_link_engine_integration_tests();
void rn_force_link_contract_tests();
void rn_force_link_editor_tests();

void rn_force_link_tests() {
	rn_force_link_service_tests();
	rn_force_link_react_host_tests();
	rn_force_link_component_view_tests();
	rn_force_link_value_codec_tests();
	rn_force_link_style_tests();
	rn_force_link_text_tests();
	rn_force_link_accessibility_tests();
	rn_force_link_engine_integration_tests();
	rn_force_link_contract_tests();
	rn_force_link_editor_tests();
}
