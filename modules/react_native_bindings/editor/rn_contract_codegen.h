#pragma once

#include "../interop/rn_component_contract.h"

// TypeScript generated from component contracts: one props type per contract,
// the registry that registers each contract component, and a first version of
// each component for the developer to fill in.
struct RNContractSources {
	String contracts;
	String registry;
};

bool rn_contract_sources(const Vector<Ref<RNComponentContract>> &p_contracts, RNContractSources &r_sources, RNError &r_error);
String rn_contract_component_source(const Ref<RNComponentContract> &p_contract);

// Generates from every contract in rn_contracts_directory() into
// react_native/project/root: godot/contracts.ts, godot/registry.ts, and
// components/<Name>.tsx where it is missing. Files are written only when
// their content changes, so Metro sees no spurious edits.
bool rn_generate_contracts(RNError &r_error);
// Whether godot/contracts.ts and godot/registry.ts hold what the contracts generate.
bool rn_generated_contracts_current();
