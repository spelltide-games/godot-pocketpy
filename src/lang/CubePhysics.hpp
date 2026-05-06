#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/templates/hash_map.hpp"
#include "godot_cpp/templates/hash_set.hpp"
#include <cassert>

namespace cube_physics {

using namespace godot;

struct Space;

const float FLOAT_MAX = 1e9f;
const float FLOAT_EPS = 1e-6f;
const float PENETRATION_CORRECTION_PERCENTAGE = 0.2f;
const Vector2i INVALID_CHUNK_POS = Vector2i(INT_MAX, INT_MAX);

using Line = Vector3[2];

struct UnitVector3 {
	int axis;
	int sign;

	UnitVector3 flip() const {
		return UnitVector3{ axis, -sign };
	}

	Vector3 to_vec3() const {
		Vector3 v(0, 0, 0);
		v[axis] = (float)sign;
		return v;
	}
};

inline bool aabb_intersects(Vector3 vmin, Vector3 vmax, Vector3 other_vmin, Vector3 other_vmax) {
	return (other_vmin.x < vmax.x) && (vmin.x < other_vmax.x) &&
			(other_vmin.y < vmax.y) && (vmin.y < other_vmax.y) &&
			(other_vmin.z < vmax.z) && (vmin.z < other_vmax.z);
}

inline Vector3 project_point_on_line(Vector3 point, Line line) {
	Vector3 d = line[1] - line[0];
	Vector3 v = point - line[0];
	float t = v.dot(d) / d.dot(d);
	t = CLAMP(t, 0.0f, 1.0f);
	return line[0] + t * d;
}

inline Vector3 cross_axis(Vector3 v, int axis) {
	// Vector3 p_with(0, 0, 0);
	// p_with[axis] = 1;
	// Vector3 ret(
	// 		(v.y * p_with.z) - (v.z * p_with.y),
	// 		(v.z * p_with.x) - (v.x * p_with.z),
	// 		(v.x * p_with.y) - (v.y * p_with.x));
	// return ret;
	if (axis == 0) {
		return Vector3(0, v.z, -v.y);
	} else if (axis == 1) {
		return Vector3(-v.z, 0, v.x);
	} else {
		return Vector3(v.y, -v.x, 0);
	}
}

struct AAFace {
	UnitVector3 normal;
	Vector3 vmin;
	Vector3 vmax;

	void get_ccw_points(Vector3 p_points[4]) const {
		static const int LUT[6][4] = {
			{ 0b000, 0b100, 0b110, 0b010 }, /* axis=0 sign=-1  (-X) */
			{ 0b001, 0b011, 0b111, 0b101 }, /* axis=0 sign=+1  (+X) */
			{ 0b000, 0b001, 0b101, 0b100 }, /* axis=1 sign=-1  (-Y) */
			{ 0b010, 0b110, 0b111, 0b011 }, /* axis=1 sign=+1  (+Y) */
			{ 0b000, 0b010, 0b011, 0b001 }, /* axis=2 sign=-1  (-Z) */
			{ 0b100, 0b101, 0b111, 0b110 }, /* axis=2 sign=+1  (+Z) */
		};
		int row = normal.axis * 2 + (normal.sign > 0 ? 1 : 0);
		for (int i = 0; i < 4; i++) {
			int mask = LUT[row][i];
			p_points[i] = Vector3(
					((mask >> 0) & 1) ? vmax.x : vmin.x,
					((mask >> 1) & 1) ? vmax.y : vmin.y,
					((mask >> 2) & 1) ? vmax.z : vmin.z);
		}
	}

	void get_parallel_edges(int axis, Line p_edges[2]) const {
		assert(axis != normal.axis);
		int t = 3 - normal.axis - axis;
		p_edges[0][0] = vmin;
		p_edges[0][1] = vmin;
		p_edges[0][1][axis] = vmax[axis];
		p_edges[1][0] = vmin;
		p_edges[1][0][t] = vmax[t];
		p_edges[1][1] = p_edges[1][0];
		p_edges[1][1][axis] = vmax[axis];
	}

	Vector3 find_closest_distance(const AAFace &other) const {
		float min_dist_squared = FLOAT_MAX;
		Vector3 min_dir;

		// a -----
		//		^
		//		| normal
		//   b -----
		for (int axis = 0; axis < 3; axis++) {
			if (axis == normal.axis)
				continue;

			Line edges_a[2], edges_b[2];
			get_parallel_edges(axis, edges_a);
			other.get_parallel_edges(axis, edges_b);

			for (int i = 0; i < 2; i++) {
				for (int j = 0; j < 2; j++) {
					for (int k = 0; k < 2; k++) {
						Vector3 vert_a = edges_a[i][k];
						Vector3 proj_b = project_point_on_line(vert_a, edges_b[j]);
						Vector3 dir = vert_a - proj_b;
						if (dir.length_squared() < min_dist_squared) {
							min_dist_squared = dir.length_squared();
							min_dir = dir;
							// print_line("vert_a: " + vert_a + ", proj_b: " + proj_b);
						}
					}
				}
			}
		}
		return min_dir;
	}
};

struct AABB {
	Vector3 vmin;
	Vector3 vmax;

	AABB() :
			vmin(Vector3(0, 0, 0)), vmax(Vector3(0, 0, 0)) {}
	AABB(Vector3 vmin, Vector3 vmax) :
			vmin(vmin), vmax(vmax) {}

	Vector3 position() const { return (vmin + vmax) * 0.5f; }
	Vector3 size() const { return vmax - vmin; }

	bool intersects(const AABB &other) const {
		return aabb_intersects(vmin, vmax, other.vmin, other.vmax);
	}

	AAFace get_face(UnitVector3 normal) const {
		AAFace face;
		face.normal = normal;
		face.vmin = vmin;
		face.vmax = vmax;
		int axis = normal.axis;
		if (normal.sign > 0) {
			face.vmin[axis] = vmax[axis];
		} else {
			face.vmax[axis] = vmin[axis];
		}
		return face;
	}

	void move_both_to_origin(AABB *p_other) {
		Vector3 offset = vmin;
		vmin -= offset;
		vmax -= offset;
		p_other->vmin -= offset;
		p_other->vmax -= offset;
	}

	float find_max_separation(const AABB &other, UnitVector3 *p_reference_normal) const {
		float sep_x1 = other.vmin.x - vmax.x;
		float sep_x2 = vmin.x - other.vmax.x;
		float sep_y1 = other.vmin.y - vmax.y;
		float sep_y2 = vmin.y - other.vmax.y;
		float sep_z1 = other.vmin.z - vmax.z;
		float sep_z2 = vmin.z - other.vmax.z;

		float max_sep = sep_x1;
		*p_reference_normal = UnitVector3{ 0, -1 };
		if (sep_x2 > max_sep) {
			max_sep = sep_x2;
			*p_reference_normal = UnitVector3{ 0, 1 };
		}
		if (sep_y1 > max_sep) {
			max_sep = sep_y1;
			*p_reference_normal = UnitVector3{ 1, -1 };
		}
		if (sep_y2 > max_sep) {
			max_sep = sep_y2;
			*p_reference_normal = UnitVector3{ 1, 1 };
		}
		if (sep_z1 > max_sep) {
			max_sep = sep_z1;
			*p_reference_normal = UnitVector3{ 2, -1 };
		}
		if (sep_z2 > max_sep) {
			max_sep = sep_z2;
			*p_reference_normal = UnitVector3{ 2, 1 };
		}
		return max_sep;
	}
};

struct Cube {
	AABB core;
	float radius;

	AABB aabb() const {
		if (radius == 0)
			return core;
		return AABB(core.vmin - Vector3(radius, radius, radius), core.vmax + Vector3(radius, radius, radius));
	}

	void move(Vector3 delta) {
		core.vmin += delta;
		core.vmax += delta;
	}

	void apply(Vector3 position, Vector3 extent, float radius01) {
		this->radius = radius01 * extent[extent.min_axis_index()];
		Vector3 offset = (extent - Vector3(1, 1, 1) * this->radius);
		this->core.vmin = position - offset;
		this->core.vmax = position + offset;
	}
};

struct Body {
	Body *prev;
	Body *next;
	void *ctx;

	Cube cube;
	Vector3 velocity;
	Vector3 instant_velocity;
	Vector2i chunk_pos;

	uint32_t layer;
	bool is_static;
	bool is_trigger;
	bool signal_enabled;
	float mass;

	Body(void *ctx, uint32_t layer, bool is_static, bool is_trigger, float mass) :
			prev(nullptr), next(nullptr), ctx(ctx), layer(layer), is_static(is_static), is_trigger(is_trigger), signal_enabled(false), mass(mass) {
		std::memset(&cube, 0, sizeof(Cube));
		velocity.zero();
		instant_velocity.zero();
		chunk_pos = INVALID_CHUNK_POS;
	}

	Vector3 position() const { return cube.core.position(); }

	bool is_moving() const {
		return velocity != Vector3(0, 0, 0) || instant_velocity != Vector3(0, 0, 0);
	}
};

struct CollisionPair {
	Body *a; // non-static
	Body *b;

	CollisionPair(Body *a, Body *b) :
			a(a), b(b) {}

	bool operator==(const CollisionPair &other) const {
		return (a == other.a && b == other.b) || (a == other.b && b == other.a);
	}

	bool operator!=(const CollisionPair &other) const {
		return !(*this == other);
	}

	bool is_trigger() const {
		return a->is_trigger || b->is_trigger;
	}

	struct Hasher {
		static uint32_t hash(const CollisionPair &pair) {
			uint64_t a_addr = reinterpret_cast<uint64_t>(pair.a);
			uint64_t b_addr = reinterpret_cast<uint64_t>(pair.b);
			return static_cast<uint32_t>(a_addr ^ b_addr);
		}
	};

	struct Info {
		Vector3 normal;
		float max_sep;
	};
};

struct Space {
	float chunk_size;
	Vector3 gravity;
	int body_count;
	uint32_t layer_masks[32];

	HashMap<Vector2i, Body *> chunks;

	HashSet<Body *> dynamic_bodies;

	HashMap<CollisionPair, CollisionPair::Info, CollisionPair::Hasher> curr_pairs;
	HashMap<CollisionPair, CollisionPair::Info, CollisionPair::Hasher> prev_pairs;

	void (*pair_added)(Space *space, Body *a, Body *b, Vector3 normal);
	void (*pair_removed)(Space *space, Body *a, Body *b);

	Space(float chunk_size) {
		this->chunk_size = chunk_size;
		this->body_count = 0;
		this->gravity = Vector3(0, 0, 0);
		for (int i = 0; i < 32; i++) {
			layer_masks[i] = 0xFFFFFFFF;
		}
		this->pair_added = nullptr;
		this->pair_removed = nullptr;
	}

	void add_curr_pair(Body *a, Body *b, Vector3 normal, float max_sep) {
		CollisionPair pair(a, b);
		if (!curr_pairs.has(pair)) {
			curr_pairs.insert(pair, CollisionPair::Info{ normal, max_sep });
		}
	}

	Body *create_body(Vector3 position, Vector3 extent, float radius01, void *ctx, uint32_t layer, bool is_static, bool is_trigger, float mass) {
		Body *body = new Body(ctx, layer, is_static, is_trigger, mass);
		body->cube.apply(position, extent, radius01);
		if (!is_static) {
			dynamic_bodies.insert(body);
		}
		update_body_chunk(body);
		body_count++;
		return body;
	}

	void update_body_chunk(Body *body) {
		Vector2i chunk_pos = to_chunk(body->position());
		if (body->chunk_pos == chunk_pos) {
			return;
		}
		remove_body_chunk(body);
		body->chunk_pos = chunk_pos;
		Body *head = chunks.has(chunk_pos) ? chunks[chunk_pos] : nullptr;
		body->next = head;
		if (head) {
			head->prev = body;
		}
		chunks[chunk_pos] = body;
	}

	void remove_body_chunk(Body *body) {
		if (body->chunk_pos == INVALID_CHUNK_POS) {
			return;
		}
		if (!body->prev) {
			if (body->next) {
				chunks[body->chunk_pos] = body->next;
			} else {
				chunks.erase(body->chunk_pos);
			}
		} else {
			body->prev->next = body->next;
		}
		if (body->next) {
			body->next->prev = body->prev;
		}
		body->prev = nullptr;
		body->next = nullptr;
		body->chunk_pos = INVALID_CHUNK_POS;
	}

	void destroy_body(Body *body) {
		remove_body_chunk(body);
		if (!body->is_static) {
			dynamic_bodies.erase(body);
		}
		body_count--;
		delete body;
	}

	Vector2i to_chunk(Vector3 position) const {
		return Vector2i(
				static_cast<int>(floor(position.x / chunk_size)),
				static_cast<int>(floor(position.z / chunk_size)));
	}

	// void point_cast(Vector3 point);
	// void ray_cast(Vector3 from, Vector3 to, float max_distance);
	// void circle_cast(Vector3 center, float radius);
	// void sphere_cast(Vector3 center, float radius);
	// void cube_cast(Vector3 vmin, Vector3 vmax);

	void step(float delta);

	~Space() {
		for (auto &[chunk_pos, body] : chunks) {
			Body *curr = body;
			while (curr) {
				Body *next = curr->next;
				delete curr;
				curr = next;
			}
		}
	}
};

// godot Node
class CubePhysicsSpace : public Node3D {
	GDCLASS(CubePhysicsSpace, Node3D);

	Space *space = nullptr;

	float chunk_size = 16.0f;

public:
	virtual void _enter_tree() override;
	virtual void _exit_tree() override;
	virtual void _physics_process(double delta) override;

	Space *get_space() const {
		return space;
	}

	float get_chunk_size() const {
		return this->chunk_size;
	}

	void set_chunk_size(float chunk_size) {
		this->chunk_size = chunk_size;
	}

	int get_body_count() const {
		return space ? space->body_count : 0;
	}

	int get_dynamic_body_count() const {
		return space ? space->dynamic_bodies.size() : 0;
	}

	int get_chunk_count() const {
		return space ? space->chunks.size() : 0;
	}

	static void _bind_methods();
};

class CubePhysicsBody : public Node3D {
	GDCLASS(CubePhysicsBody, Node3D);

	Body *body = nullptr;

	Vector3 extent = Vector3(1, 1, 1);
	float radius01 = 0.0f;

	bool signal_enabled = false;

	uint32_t layer = 0;
	bool is_static = false;
	bool is_trigger = false;
	float mass = 1.0f;

public:
	Vector3 get_extent() const {
		return this->extent;
	}

	float get_radius01() const {
		return this->radius01;
	}

	void set_extent(Vector3 extent) {
		this->extent = extent;
	}

	void set_radius01(float radius01) {
		this->radius01 = radius01;
	}

	bool get_signal_enabled() const {
		return this->signal_enabled;
	}

	void set_signal_enabled(bool signal_enabled) {
		this->signal_enabled = signal_enabled;
	}

	uint32_t get_layer() const {
		return this->layer;
	}

	void set_layer(uint32_t layer) {
		this->layer = layer;
	}

	bool get_is_static() const {
		return this->is_static;
	}

	void set_is_static(bool is_static) {
		this->is_static = is_static;
	}

	bool get_is_trigger() const {
		return this->is_trigger;
	}

	void set_is_trigger(bool is_trigger) {
		this->is_trigger = is_trigger;
	}

	float get_mass() const {
		return this->mass;
	}

	void set_mass(float mass) {
		this->mass = mass;
	}

	Vector3 get_velocity() const {
		if (body) {
			return body->velocity;
		} else {
			return Vector3(0, 0, 0);
		}
	}

	void set_velocity(Vector3 velocity) {
		if (body) {
			body->velocity = velocity;
		}
	}

	Vector3 get_instant_velocity() const {
		if (body) {
			return body->instant_velocity;
		} else {
			return Vector3(0, 0, 0);
		}
	}

	void set_instant_velocity(Vector3 instant_velocity) {
		if (body) {
			body->instant_velocity = instant_velocity;
		}
	}

	Vector2i get_chunk_pos() const {
		if (body) {
			return body->chunk_pos;
		} else {
			return INVALID_CHUNK_POS;
		}
	}

	Space *get_space() const {
		Node *node = get_parent();
		while (node) {
			CubePhysicsSpace *space = Object::cast_to<CubePhysicsSpace>(node);
			if (space) {
				return space->get_space();
			}
			node = node->get_parent();
		}
		return nullptr;
	}

	virtual void _enter_tree() override;
	virtual void _exit_tree() override;

	static void _bind_methods();
};

} // namespace cube_physics