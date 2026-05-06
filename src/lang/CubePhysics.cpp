#include "CubePhysics.hpp"
#include "godot_cpp/classes/engine.hpp"
#include <cassert>

namespace cube_physics {

static void broad_phase_query(Space *space, const AABB &aabb, uint32_t layer_mask, void *ctx, void (*callback)(Space *space, Body *candidate, void *ctx)) {
	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			Vector2i chunk_pos = space->to_chunk(aabb.position()) + Vector2i(i, j);
			Body **p_candidates = space->chunks.getptr(chunk_pos);
			if (p_candidates) {
				Body *candidate = *p_candidates;
				while (candidate) {
					if (layer_mask & (1U << candidate->layer)) {
						if (aabb.intersects(candidate->cube.aabb())) {
							callback(space, candidate, ctx);
						}
					}
					candidate = candidate->next;
				}
			}
		}
	}
}

void Space::step(float delta) {
	// find collision pairs
	for (Body *a : dynamic_bodies) {
		assert(!a->is_static);

		if (!a->is_moving()) {
			continue;
		}

		broad_phase_query(this, a->cube.aabb(), layer_masks[a->layer], (void *)a, [](Space *space, Body *candidate, void *ctx) {
			Body *a = (Body *)ctx;
			Body *b = candidate;

			if (a == b) {
				return;
			}

			// narrow phase
			AABB a_core = a->cube.core;
			AABB b_core = b->cube.core;
			a_core.move_both_to_origin(&b_core);

			UnitVector3 n;
			float max_sep = a_core.find_max_separation(b_core, &n);

			if (max_sep < 0) {
				space->add_curr_pair(a, b, n.to_vec3(), max_sep);
				// print_line("max_sep: " + rtos(max_sep) + ", n: " + n.to_vec3());
			} else {
				AAFace a_face = a_core.get_face(n.flip());
				AAFace b_face = b_core.get_face(n);

				Vector3 n_alt = a_face.find_closest_distance(b_face);
				float n_alt_length = n_alt.length();
				max_sep = n_alt_length - a->cube.radius - b->cube.radius;

				if (max_sep < 0) {
					if (n_alt_length > FLOAT_EPS) {
						n_alt /= n_alt_length;
					} else {
						n_alt = a->position() - b->position();
						n_alt /= n_alt.length();
					}
					space->add_curr_pair(a, b, n_alt, max_sep);
					// print_line("max_sep_alt: " + rtos(max_sep) + ", n_alt: " + n_alt);
				}
			}
		});
	}

	// resolve collisions
	for (const auto &[pair, info] : curr_pairs) {
		if (pair.is_trigger())
			continue;

		// a: incident body
		// b: reference body
		assert(info.max_sep < 0);

		Vector3 n = info.normal;
		const float e = 1.0f; // restitution

		// contact separation
		Vector3 correction = n * (-info.max_sep + PENETRATION_SLOP);
		correction *= PENETRATION_CORRECTION_PERCENTAGE;
		Vector3 v_bias = correction / delta;

		if (pair.b->is_static) {
			pair.a->instant_velocity += v_bias;
			// impulse-based collision resolution
			Vector3 v_rel = pair.a->velocity;
			float v_rel_n = v_rel.dot(n);
			if (v_rel_n < 0) {
				Vector3 v_delta = -(1 + e) * v_rel_n * n;
				pair.a->velocity += v_delta;
			}
		} else {
			float mass_ratio = pair.a->mass / (pair.a->mass + pair.b->mass);
			pair.a->instant_velocity += v_bias * (1 - mass_ratio);
			pair.b->instant_velocity -= v_bias * mass_ratio;
			// impulse-based collision resolution
			Vector3 v_rel = pair.a->velocity - pair.b->velocity;
			float v_rel_n = v_rel.dot(n);
			if (v_rel_n < 0) {
				Vector3 v_delta = -(1 + e) * v_rel_n * n;
				pair.a->velocity += v_delta * (1 - mass_ratio);
				pair.b->velocity -= v_delta * mass_ratio;
			}
		}
	}

	// send signals
	if (pair_added) {
		for (const auto &[pair, info] : curr_pairs) {
			assert(!pair.a->is_removed);
			assert(!pair.b->is_removed);
			if (!prev_pairs.has(pair)) {
				pair_added(this, pair.a, pair.b, info.normal);
			}
		}
	}
	if (pair_removed) {
		for (const auto &[pair, info] : prev_pairs) {
			if (!pair.a->is_removed && !pair.b->is_removed) {
				if (!curr_pairs.has(pair)) {
					pair_removed(this, pair.a, pair.b);
				}
			}
		}
	}
	prev_pairs = std::move(curr_pairs);
	curr_pairs.clear();

	// move bodies by its velocity
	for (Body *body : dynamic_bodies) {
		if (!body->is_moving()) {
			continue;
		}

		// body->velocity += this->gravity * delta;
		Vector3 total_vel = body->velocity + body->instant_velocity;
		body->instant_velocity.zero();
		body->cube.move(total_vel * delta);
		update_body_chunk(body);
	}

	// delete removed bodies
	for (Body *body : prev_removed_bodies) {
		delete body;
	}
	prev_removed_bodies.clear();
}

// godot Node
void CubePhysicsSpace::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_get_chunk_size"), &CubePhysicsSpace::get_chunk_size);
	ClassDB::bind_method(D_METHOD("_set_chunk_size", "chunk_size"), &CubePhysicsSpace::set_chunk_size);
	ClassDB::bind_method(D_METHOD("_get_body_count"), &CubePhysicsSpace::get_body_count);
	ClassDB::bind_method(D_METHOD("_get_dynamic_body_count"), &CubePhysicsSpace::get_dynamic_body_count);
	ClassDB::bind_method(D_METHOD("_get_chunk_count"), &CubePhysicsSpace::get_chunk_count);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "chunk_size"), "_set_chunk_size", "_get_chunk_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "body_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "_get_body_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "dynamic_body_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "_get_dynamic_body_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "_get_chunk_count");
}

void CubePhysicsSpace::_enter_tree() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	space = new Space(chunk_size);
	space->pair_added = [](Space *space, Body *a, Body *b, Vector3 normal) {
		CubePhysicsBody *a_node = static_cast<CubePhysicsBody *>(a->ctx);
		CubePhysicsBody *b_node = static_cast<CubePhysicsBody *>(b->ctx);
		if (a_node->is_signal_enabled()) {
			a_node->emit_signal("body_entered", b_node, normal);
		}
		if (b_node->is_signal_enabled()) {
			b_node->emit_signal("body_entered", a_node, -normal);
		}
	};
	space->pair_removed = [](Space *space, Body *a, Body *b) {
		CubePhysicsBody *a_node = static_cast<CubePhysicsBody *>(a->ctx);
		CubePhysicsBody *b_node = static_cast<CubePhysicsBody *>(b->ctx);
		if (a_node->is_signal_enabled()) {
			a_node->emit_signal("body_exited", b_node);
		}
		if (b_node->is_signal_enabled()) {
			b_node->emit_signal("body_exited", a_node);
		}
	};
}

void CubePhysicsSpace::_exit_tree() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	delete space;
	space = nullptr;
}

void CubePhysicsSpace::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (!is_visible()) {
		return;
	}
	space->step((float)delta);
	for (Body *body : space->dynamic_bodies) {
		CubePhysicsBody *node = static_cast<CubePhysicsBody *>(body->ctx);
		node->set_global_position(body->position());
	}
}

void CubePhysicsBody::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_get_extent"), &CubePhysicsBody::get_extent);
	ClassDB::bind_method(D_METHOD("_get_radius01"), &CubePhysicsBody::get_radius01);
	ClassDB::bind_method(D_METHOD("_set_extent", "extent"), &CubePhysicsBody::set_extent);
	ClassDB::bind_method(D_METHOD("_set_radius01", "radius01"), &CubePhysicsBody::set_radius01);
	ClassDB::bind_method(D_METHOD("_is_signal_enabled"), &CubePhysicsBody::is_signal_enabled);
	ClassDB::bind_method(D_METHOD("_set_signal_enabled", "signal_enabled"), &CubePhysicsBody::set_signal_enabled);

	ClassDB::bind_method(D_METHOD("_is_static"), &CubePhysicsBody::is_static);
	ClassDB::bind_method(D_METHOD("_set_static", "is_static"), &CubePhysicsBody::set_static);
	ClassDB::bind_method(D_METHOD("_is_trigger"), &CubePhysicsBody::is_trigger);
	ClassDB::bind_method(D_METHOD("_set_trigger", "is_trigger"), &CubePhysicsBody::set_trigger);
	ClassDB::bind_method(D_METHOD("_get_layer"), &CubePhysicsBody::get_layer);
	ClassDB::bind_method(D_METHOD("_set_layer", "layer"), &CubePhysicsBody::set_layer);
	ClassDB::bind_method(D_METHOD("_get_mass"), &CubePhysicsBody::get_mass);
	ClassDB::bind_method(D_METHOD("_set_mass", "mass"), &CubePhysicsBody::set_mass);

	ClassDB::bind_method(D_METHOD("_get_chunk_pos"), &CubePhysicsBody::get_chunk_pos);
	ClassDB::bind_method(D_METHOD("_get_velocity"), &CubePhysicsBody::get_velocity);
	ClassDB::bind_method(D_METHOD("_set_velocity", "velocity"), &CubePhysicsBody::set_velocity);
	ClassDB::bind_method(D_METHOD("_get_instant_velocity"), &CubePhysicsBody::get_instant_velocity);
	ClassDB::bind_method(D_METHOD("_set_instant_velocity", "instant_velocity"), &CubePhysicsBody::set_instant_velocity);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "extent"), "_set_extent", "_get_extent");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius01", PROPERTY_HINT_RANGE, "0.0,1.0"), "_set_radius01", "_get_radius01");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_signal_enabled"), "_set_signal_enabled", "_is_signal_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_static"), "_set_static", "_is_static");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_trigger"), "_set_trigger", "_is_trigger");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layer"), "_set_layer", "_get_layer");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass"), "_set_mass", "_get_mass");

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "chunk_pos", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "_get_chunk_pos");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "_set_velocity", "_get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "instant_velocity", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "_set_instant_velocity", "_get_instant_velocity");

	// signal
	ADD_SIGNAL(MethodInfo("body_entered", PropertyInfo(Variant::OBJECT, "other"), PropertyInfo(Variant::VECTOR3, "normal")));
	ADD_SIGNAL(MethodInfo("body_exited", PropertyInfo(Variant::OBJECT, "other")));
}

void CubePhysicsBody::_enter_tree() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	Space *space = get_space();
	if (!space) {
		ERR_PRINT("CubePhysicsBody must be a child of CubePhysicsSpace");
		return;
	}
	body = space->create_body(get_global_position(), extent, radius01, this, _is_static, _is_trigger, _layer, _mass);
}

void CubePhysicsBody::_exit_tree() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	Space *space = get_space();
	if (!space) {
		ERR_PRINT("CubePhysicsBody must be a child of CubePhysicsSpace");
		return;
	}
	space->destroy_body(body);
	body = nullptr;
}

} // namespace cube_physics