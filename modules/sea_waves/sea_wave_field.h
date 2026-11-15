/**************************************************************************/
/*  sea_wave_field.h                                                      */
/**************************************************************************/
/* Deep Salvage: the sea's long waves on the CPU.                         */
/*                                                                        */
/* A low-resolution twin of the game's GPU FFT ocean (the compute shaders */
/* of shaders/water/compute/), for what must agree between the games and  */
/* the zone server: ships, floating things. Same spectrum (JONSWAP x TMA, */
/* Hasselmann spreading), same hash, same dispersion and phase snapping,  */
/* same per-cascade time offsets, so the waves are the ones drawn, minus  */
/* the shortest ones the grid cannot hold. Several sea states can be held */
/* at once (one per sea zone) and read mixed by place: the FFT is linear. */
/* Reading follows OceanWaveQuery and the water's vertex shader: the      */
/* weave (two readings of each cascade, the sideways push), choppy waves  */
/* inverted by fixed-point iteration, the water's orbital velocity.       */
/* Keep every constant in step with the shaders and core/ocean/ scripts. */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h"

class SeaWaveField : public RefCounted {
	GDCLASS(SeaWaveField, RefCounted);

public:
	static constexpr int MAX_SLOTS = 4;

private:
	struct Cascade {
		float length = 89.0f;
		int id = 0; // index in the GPU simulation (weave constants, hash)
		float time_offset = 0.0f;
	};

	struct Slot {
		bool loaded = false;
		// Per grid cell, in FFT order: h0(k), conj(h0(-k)), snapped omega, unit wave vector.
		LocalVector<float> h0_re, h0_im, h0m_re, h0m_im, omega, kx, kz;
		float height_gain = 1.0f;
		float side_gain = 1.0f;
		// The fields at the grid points (row-major, z rows): (dx, height, dz) and their rate.
		LocalVector<Vector3> displacement;
		LocalVector<Vector3> velocity;
	};

	struct State {
		Slot slots[MAX_SLOTS];
	};

	int resolution = 32;
	uint32_t seed = 1337;
	float period = 256.0f;
	float gravity = 9.81f;
	float dispersion_depth = 40.0f;
	LocalVector<Cascade> cascades;
	LocalVector<State> states;
	double updated_time = -1.0;

	// Scratch for the transforms (no allocation per update).
	LocalVector<float> a_re, a_im, b_re, b_im, c_re, c_im;
	LocalVector<float> twiddle_re, twiddle_im;
	LocalVector<int> bit_reverse;

	// Reading context.
	int mix_from = 0;
	int mix_to = 0;
	float mix_t = 0.0f;
	float gains[MAX_SLOTS] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float shares[MAX_SLOTS] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float scale = 1.0f;
	Vector2 group_offset;
	float warp_strength = 1.0f;
	Vector2 push;
	Vector2 turn_x = Vector2(1, 0);
	Vector2 turn_y = Vector2(0, 1);
	Vector2 last_grid_point;
	PackedVector2Array batch_grid_points;

	void _fill_spectrum(Slot &r_slot, const Cascade &p_cascade, const PackedFloat32Array &p_params);
	void _evolve(Slot &r_slot, const Cascade &p_cascade, double p_time);
	void _inverse_fft_2d(LocalVector<float> &r_re, LocalVector<float> &r_im);
	void _inverse_fft_line(float *r_re, float *r_im, int p_stride);
	Vector3 _fetch(const LocalVector<Vector3> &p_grid, float p_u, float p_v) const;
	Vector3 _fetch_mixed(int p_slot, float p_u, float p_v, bool p_velocity) const;
	Vector3 _sum(const Vector2 &p_point, bool p_velocity) const;
	Vector3 _displacement(const Vector2 &p_point, bool p_velocity) const;

protected:
	static void _bind_methods();

public:
	// Cascade lengths (m), their GPU indices and time offsets (s), as OceanSimulation uploads them.
	void setup(const PackedFloat32Array &p_lengths, const PackedInt32Array &p_ids, const PackedFloat32Array &p_time_offsets,
			int p_resolution, int p_seed, float p_period, float p_gravity, float p_dispersion_depth);
	void set_state_count(int p_count);
	int get_state_count() const;
	// p_params: alpha, peak omega, wind speed, wind angle (rad), depth, swell, detail, spread, amplitude.
	void set_spectrum(int p_state, int p_slot, const PackedFloat32Array &p_params);
	// What OceanSimulation scales the drawn waves by (steepness budget, displacement weight) and
	// the sideways offsets by (the same, the chop limit and the choppiness).
	void set_gains(int p_state, int p_slot, float p_height_gain, float p_side_gain);
	bool is_loaded(int p_state) const;
	// The waves at wave clock p_time (s). Cheap to call again with the same time.
	void update(double p_time);

	// Reading context: the place's sea (two states mixed), the per-slot gains (wave groups,
	// shoaling) and weave shares, the shore's shelter, the weave's push.
	void set_mix(int p_from, int p_to, float p_t);
	void set_context(const PackedFloat32Array &p_gains, const PackedFloat32Array &p_shares, float p_scale);
	void set_warp_params(const Vector2 &p_group_offset, float p_strength);
	void set_warp(const Vector2 &p_xz);

	// The weighted sum of the cascades read at grid point p (before the weave's turn) and its rate.
	Vector3 sea(const Vector2 &p_point) const;
	Vector3 sea_velocity(const Vector2 &p_point) const;
	Vector2 turned(const Vector2 &p_vector) const;
	Vector2 get_push() const;
	// The surface above xz: x, y, z the water's velocity, w the height. Sets the warp at xz.
	Vector4 surface(const Vector2 &p_xz, int p_iterations = 2);
	Vector2 get_last_grid_point() const;
	PackedVector4Array surfaces(const PackedVector2Array &p_points, int p_iterations = 2);
	PackedVector2Array get_batch_grid_points() const;

	// Tests: the Gaussian pair of a signed frequency, as spectrum_init.glsl draws it.
	static Vector2 gaussian_pair(int p_mx, int p_mz, int p_cascade, int p_seed);
	PackedVector3Array get_grid(int p_state, int p_slot, bool p_velocity) const;
};
