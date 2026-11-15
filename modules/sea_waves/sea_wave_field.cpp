/**************************************************************************/
/*  sea_wave_field.cpp                                                    */
/**************************************************************************/
/* Deep Salvage: the sea's long waves on the CPU. See sea_wave_field.h.   */
/**************************************************************************/

#include "sea_wave_field.h"

#include "core/object/class_db.h"

#include <cmath>

namespace {

constexpr float PI_F = 3.14159265358979f;
constexpr float TAU_F = 6.28318530717959f;

// Frequencies are hashed signed, biased to stay positive (spectrum_init.glsl, HASH_BIAS): the
// same wave has the same random draw at any grid size, so every water quality and this twin
// share one sea.
constexpr int HASH_BIAS = 32768;

// WaveWeave (core/ocean/wave_weave.gd) and wave_weave.gdshaderinc: keep in step.
const Vector2 WEAVE_TURN[4] = { Vector2(0.829038f, -0.559193f), Vector2(0.681998f, 0.731354f), Vector2(1.0f, 0.0f),
	Vector2(0.891007f, 0.453990f) };
const Vector2 WEAVE_SHIFT[4] = { Vector2(0.37f, 0.61f), Vector2(0.73f, 0.19f), Vector2(0.0f, 0.0f), Vector2(0.43f, 0.27f) };
constexpr float WARP_STEP = TAU_F / 10000.0f;
const Vector3 WARP[3] = { Vector3(7.0f, 3.0f, 44.0f), Vector3(-8.0f, 14.0f, 15.0f), Vector3(-31.0f, -22.0f, 4.5f) };
constexpr float WARP_DRIFT = 0.2f;

uint32_t pcg_hash(uint32_t p_v) {
	uint32_t state = p_v * 747796405u + 2891336453u;
	uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float hash_to_unit(uint32_t p_h) {
	return (float(p_h >> 8u) + 0.5f) * (1.0f / 16777216.0f);
}

float safe_tanh(float p_x) {
	return std::tanh(std::fmin(std::fmax(p_x, -10.0f), 10.0f));
}

int frequency_of(int p_index, int p_n) {
	return ((p_index + p_n / 2) % p_n) - p_n / 2;
}

struct SpectrumParams {
	float tile_length;
	float alpha;
	float peak_omega;
	float wind_speed;
	float wind_angle;
	float depth;
	float swell;
	float detail;
	float spread;
	float gravity;
	float amplitude;
};

// spectrum_init.glsl, function by function.
void dispersion(const SpectrumParams &p, float p_k, float &r_omega, float &r_d_omega) {
	float a = p_k * p.depth;
	float b = safe_tanh(a);
	r_omega = std::sqrt(p.gravity * p_k * b);
	r_d_omega = 0.5f * p.gravity * (b + a * (1.0f - b * b)) / r_omega;
}

float longuet_higgins_normalization(float p_s) {
	float a = std::sqrt(p_s);
	return (p_s < 0.4f) ? (0.5f / PI_F) + p_s * (0.220636f + p_s * (-0.109f + p_s * 0.090f))
						: (1.0f / std::sqrt(PI_F)) * (a * 0.5f + (1.0f / a) * 0.0625f);
}

float hasselmann_spread(const SpectrumParams &p, float p_omega, float p_theta) {
	float q = p_omega / p.peak_omega;
	float s = (p_omega <= p.peak_omega) ? 6.97f * std::pow(std::fabs(q), 4.06f)
										: 9.77f * std::pow(std::fabs(q), -2.33f - 1.45f * (p.wind_speed * p.peak_omega / p.gravity - 1.17f));
	s += 16.0f * safe_tanh(p.peak_omega / p_omega) * p.swell * p.swell;
	return longuet_higgins_normalization(s) * std::pow(std::fabs(std::cos((p_theta - p.wind_angle) * 0.5f)), 2.0f * s);
}

float tma_spectrum(const SpectrumParams &p, float p_omega) {
	float wp = p.peak_omega;
	float sigma = (p_omega <= wp) ? 0.07f : 0.09f;
	float r = std::exp(-(p_omega - wp) * (p_omega - wp) / (2.0f * sigma * sigma * wp * wp));
	float jonswap = (p.alpha * p.gravity * p.gravity) / std::pow(p_omega, 5.0f) * std::exp(-1.25f * std::pow(wp / p_omega, 4.0f)) * std::pow(3.3f, r);
	float wh = std::fmin(p_omega * std::sqrt(p.depth / p.gravity), 2.0f);
	float depth_attenuation = (wh <= 1.0f) ? 0.5f * wh * wh : 1.0f - 0.5f * (2.0f - wh) * (2.0f - wh);
	return jonswap * depth_attenuation;
}

Vector2 h0_at(const SpectrumParams &p, int p_mx, int p_mz, float p_kx, float p_kz, int p_cascade, uint32_t p_seed) {
	float k = std::sqrt(p_kx * p_kx + p_kz * p_kz);
	if (k < 1e-6f) {
		return Vector2();
	}
	float dk = TAU_F / p.tile_length;
	float omega, d_omega;
	dispersion(p, k, omega, d_omega);
	float theta = std::atan2(p_kx, p_kz);
	float isotropic = 0.5f / PI_F;
	float directional = (isotropic + (hasselmann_spread(p, omega, theta) - isotropic) * (1.0f - p.spread)) * std::exp(-(1.0f - p.detail) * (1.0f - p.detail) * k * k);
	float energy = tma_spectrum(p, omega) * directional * d_omega / k * dk * dk;
	Vector2 g = SeaWaveField::gaussian_pair(p_mx, p_mz, p_cascade, int(p_seed));
	return g * (std::sqrt(2.0f * energy) * p.amplitude);
}

} // namespace

Vector2 SeaWaveField::gaussian_pair(int p_mx, int p_mz, int p_cascade, int p_seed) {
	uint32_t tx = uint32_t(p_mx + HASH_BIAS);
	uint32_t tz = uint32_t(p_mz + HASH_BIAS);
	uint32_t h1 = pcg_hash(tx + pcg_hash(tz + pcg_hash(uint32_t(p_cascade) + pcg_hash(uint32_t(p_seed)))));
	uint32_t h2 = pcg_hash(h1 ^ 0x9E3779B9u);
	float u1 = hash_to_unit(h1);
	float u2 = hash_to_unit(h2);
	float r = std::sqrt(-2.0f * std::log(u1));
	return Vector2(r * std::cos(TAU_F * u2), r * std::sin(TAU_F * u2));
}

void SeaWaveField::setup(const PackedFloat32Array &p_lengths, const PackedInt32Array &p_ids, const PackedFloat32Array &p_time_offsets,
		int p_resolution, int p_seed, float p_period, float p_gravity, float p_dispersion_depth) {
	ERR_FAIL_COND_MSG(p_lengths.size() == 0 || p_lengths.size() > MAX_SLOTS, "SeaWaveField: 1 to 4 cascades.");
	ERR_FAIL_COND(p_ids.size() != p_lengths.size() || p_time_offsets.size() != p_lengths.size());
	ERR_FAIL_COND_MSG(p_resolution < 4 || p_resolution > 256 || (p_resolution & (p_resolution - 1)) != 0,
			"SeaWaveField: the resolution must be a power of two from 4 to 256.");
	resolution = p_resolution;
	seed = uint32_t(p_seed);
	period = p_period;
	gravity = p_gravity;
	dispersion_depth = p_dispersion_depth;
	cascades.resize(p_lengths.size());
	for (uint32_t i = 0; i < cascades.size(); i++) {
		cascades[i].length = p_lengths[i];
		cascades[i].id = p_ids[i];
		cascades[i].time_offset = p_time_offsets[i];
	}
	int cells = resolution * resolution;
	a_re.resize(cells);
	a_im.resize(cells);
	b_re.resize(cells);
	b_im.resize(cells);
	c_re.resize(cells);
	c_im.resize(cells);
	twiddle_re.resize(resolution / 2);
	twiddle_im.resize(resolution / 2);
	for (int j = 0; j < resolution / 2; j++) {
		float angle = TAU_F * float(j) / float(resolution); // positive sign: inverse transform
		twiddle_re[j] = std::cos(angle);
		twiddle_im[j] = std::sin(angle);
	}
	bit_reverse.resize(resolution);
	int bits = 0;
	while ((1 << bits) < resolution) {
		bits++;
	}
	for (int i = 0; i < resolution; i++) {
		int r = 0;
		for (int b = 0; b < bits; b++) {
			if (i & (1 << b)) {
				r |= 1 << (bits - 1 - b);
			}
		}
		bit_reverse[i] = r;
	}
	for (uint32_t s = 0; s < states.size(); s++) {
		for (int c = 0; c < MAX_SLOTS; c++) {
			states[s].slots[c] = Slot();
		}
	}
	updated_time = -1.0;
}

void SeaWaveField::set_state_count(int p_count) {
	ERR_FAIL_COND(p_count < 0 || p_count > 64);
	states.resize(p_count);
	updated_time = -1.0;
}

int SeaWaveField::get_state_count() const {
	return int(states.size());
}

void SeaWaveField::_fill_spectrum(Slot &r_slot, const Cascade &p_cascade, const PackedFloat32Array &p_params) {
	SpectrumParams p;
	p.tile_length = p_cascade.length;
	p.alpha = p_params[0];
	p.peak_omega = p_params[1];
	p.wind_speed = p_params[2];
	p.wind_angle = p_params[3];
	p.depth = p_params[4];
	p.swell = p_params[5];
	p.detail = p_params[6];
	p.spread = p_params[7];
	p.amplitude = p_params[8];
	p.gravity = gravity;
	int n = resolution;
	int cells = n * n;
	r_slot.h0_re.resize(cells);
	r_slot.h0_im.resize(cells);
	r_slot.h0m_re.resize(cells);
	r_slot.h0m_im.resize(cells);
	r_slot.omega.resize(cells);
	r_slot.kx.resize(cells);
	r_slot.kz.resize(cells);
	r_slot.displacement.resize(cells);
	r_slot.velocity.resize(cells);
	float dk = TAU_F / p_cascade.length;
	float omega_0 = TAU_F / period;
	for (int iz = 0; iz < n; iz++) {
		for (int ix = 0; ix < n; ix++) {
			int i = iz * n + ix;
			int mx = frequency_of(ix, n);
			int mz = frequency_of(iz, n);
			float kx = mx * dk;
			float kz = mz * dk;
			float k = std::sqrt(kx * kx + kz * kz);
			// The grid's Nyquist line holds no wave: on the GPU's bigger grid the frequencies
			// +n/2 and -n/2 are two waves, here they would fold into one.
			if (k < 1e-6f || mx == -n / 2 || mz == -n / 2) {
				r_slot.h0_re[i] = r_slot.h0_im[i] = r_slot.h0m_re[i] = r_slot.h0m_im[i] = 0.0f;
				r_slot.omega[i] = r_slot.kx[i] = r_slot.kz[i] = 0.0f;
				continue;
			}
			Vector2 h0 = h0_at(p, mx, mz, kx, kz, p_cascade.id, seed);
			Vector2 h0_minus = h0_at(p, -mx, -mz, -kx, -kz, p_cascade.id, seed);
			r_slot.h0_re[i] = h0.x;
			r_slot.h0_im[i] = h0.y;
			r_slot.h0m_re[i] = h0_minus.x;
			r_slot.h0m_im[i] = -h0_minus.y;
			// fft_rows.glsl: dispersion at one depth for all seas, snapped to whole turns per period.
			float omega = std::sqrt(gravity * k * safe_tanh(k * dispersion_depth));
			r_slot.omega[i] = std::floor(omega / omega_0 + 0.5f) * omega_0;
			r_slot.kx[i] = kx / k;
			r_slot.kz[i] = kz / k;
		}
	}
	r_slot.loaded = true;
}

void SeaWaveField::set_spectrum(int p_state, int p_slot, const PackedFloat32Array &p_params) {
	ERR_FAIL_INDEX(p_state, int(states.size()));
	ERR_FAIL_INDEX(p_slot, int(cascades.size()));
	ERR_FAIL_COND_MSG(p_params.size() < 9, "SeaWaveField.set_spectrum: 9 parameters.");
	_fill_spectrum(states[p_state].slots[p_slot], cascades[p_slot], p_params);
	updated_time = -1.0;
}

void SeaWaveField::set_gains(int p_state, int p_slot, float p_height_gain, float p_side_gain) {
	ERR_FAIL_INDEX(p_state, int(states.size()));
	ERR_FAIL_INDEX(p_slot, int(cascades.size()));
	Slot &slot = states[p_state].slots[p_slot];
	if (slot.height_gain != p_height_gain || slot.side_gain != p_side_gain) {
		slot.height_gain = p_height_gain;
		slot.side_gain = p_side_gain;
		updated_time = -1.0;
	}
}

bool SeaWaveField::is_loaded(int p_state) const {
	ERR_FAIL_INDEX_V(p_state, int(states.size()), false);
	for (uint32_t c = 0; c < cascades.size(); c++) {
		if (!states[p_state].slots[c].loaded) {
			return false;
		}
	}
	return !cascades.is_empty();
}

void SeaWaveField::_inverse_fft_line(float *r_re, float *r_im, int p_stride) {
	int n = resolution;
	for (int i = 0; i < n; i++) {
		int j = bit_reverse[i];
		if (j > i) {
			SWAP(r_re[i * p_stride], r_re[j * p_stride]);
			SWAP(r_im[i * p_stride], r_im[j * p_stride]);
		}
	}
	for (int size = 2; size <= n; size <<= 1) {
		int half = size >> 1;
		int step = n / size;
		for (int start = 0; start < n; start += size) {
			for (int k = 0; k < half; k++) {
				float wr = twiddle_re[k * step];
				float wi = twiddle_im[k * step];
				int a = (start + k) * p_stride;
				int b = (start + k + half) * p_stride;
				float tr = r_re[b] * wr - r_im[b] * wi;
				float ti = r_re[b] * wi + r_im[b] * wr;
				r_re[b] = r_re[a] - tr;
				r_im[b] = r_im[a] - ti;
				r_re[a] += tr;
				r_im[a] += ti;
			}
		}
	}
}

void SeaWaveField::_inverse_fft_2d(LocalVector<float> &r_re, LocalVector<float> &r_im) {
	int n = resolution;
	// Along x (each row), then along z (each column).
	for (int row = 0; row < n; row++) {
		_inverse_fft_line(r_re.ptr() + row * n, r_im.ptr() + row * n, 1);
	}
	for (int column = 0; column < n; column++) {
		_inverse_fft_line(r_re.ptr() + column, r_im.ptr() + column, n);
	}
}

void SeaWaveField::_evolve(Slot &r_slot, const Cascade &p_cascade, double p_time) {
	int cells = resolution * resolution;
	// fft_rows.glsl: the clock wrapped to the loop period, plus the cascade's offset.
	float time = float(std::fmod(p_time, double(period)));
	if (time < 0.0f) {
		time += period;
	}
	time += p_cascade.time_offset;
	for (int i = 0; i < cells; i++) {
		float omega = r_slot.omega[i];
		if (omega == 0.0f) {
			a_re[i] = a_im[i] = b_re[i] = b_im[i] = c_re[i] = c_im[i] = 0.0f;
			continue;
		}
		float phase = omega * time;
		float c = std::cos(phase);
		float s = std::sin(phase);
		float pr = r_slot.h0_re[i] * c - r_slot.h0_im[i] * s; // h0 e^(i phase)
		float pi = r_slot.h0_re[i] * s + r_slot.h0_im[i] * c;
		float mr = r_slot.h0m_re[i] * c + r_slot.h0m_im[i] * s; // conj(h0(-k)) e^(-i phase)
		float mi = -r_slot.h0m_re[i] * s + r_slot.h0m_im[i] * c;
		float h_re = pr + mr;
		float h_im = pi + mi;
		// Its rate: i omega (h0 e^(i phase) - conj(h0(-k)) e^(-i phase)).
		float hv_re = -omega * (pi - mi);
		float hv_im = omega * (pr - mr);
		float kx = r_slot.kx[i];
		float kz = r_slot.kz[i];
		// i k/|k| times the height: the choppy offsets.
		float dx_re = -h_im * kx, dx_im = h_re * kx;
		float dz_re = -h_im * kz, dz_im = h_re * kz;
		float vx_re = -hv_im * kx, vx_im = hv_re * kx;
		float vz_re = -hv_im * kz, vz_im = hv_re * kz;
		// Two real fields per transform (both Hermitian): A + iB gives a + ib.
		a_re[i] = dx_re - h_im;
		a_im[i] = dx_im + h_re;
		b_re[i] = dz_re - hv_im;
		b_im[i] = dz_im + hv_re;
		c_re[i] = vx_re - vz_im;
		c_im[i] = vx_im + vz_re;
	}
	_inverse_fft_2d(a_re, a_im);
	_inverse_fft_2d(b_re, b_im);
	_inverse_fft_2d(c_re, c_im);
	float side = r_slot.side_gain;
	float up = r_slot.height_gain;
	for (int i = 0; i < cells; i++) {
		r_slot.displacement[i] = Vector3(a_re[i] * side, a_im[i] * up, b_re[i] * side);
		r_slot.velocity[i] = Vector3(c_re[i] * side, b_im[i] * up, c_im[i] * side);
	}
}

void SeaWaveField::update(double p_time) {
	if (p_time == updated_time) {
		return;
	}
	updated_time = p_time;
	for (uint32_t s = 0; s < states.size(); s++) {
		for (uint32_t c = 0; c < cascades.size(); c++) {
			Slot &slot = states[s].slots[c];
			if (slot.loaded) {
				_evolve(slot, cascades[c], p_time);
			}
		}
	}
}

void SeaWaveField::set_mix(int p_from, int p_to, float p_t) {
	ERR_FAIL_INDEX(p_from, int(states.size()));
	ERR_FAIL_INDEX(p_to, int(states.size()));
	mix_from = p_from;
	mix_to = p_to;
	mix_t = CLAMP(p_t, 0.0f, 1.0f);
}

void SeaWaveField::set_context(const PackedFloat32Array &p_gains, const PackedFloat32Array &p_shares, float p_scale) {
	for (int s = 0; s < MAX_SLOTS; s++) {
		gains[s] = s < p_gains.size() ? p_gains[s] : 0.0f;
		shares[s] = s < p_shares.size() ? p_shares[s] : 0.0f;
	}
	scale = p_scale;
}

void SeaWaveField::set_warp_params(const Vector2 &p_group_offset, float p_strength) {
	group_offset = p_group_offset;
	warp_strength = p_strength;
}

void SeaWaveField::set_warp(const Vector2 &p_xz) {
	push = Vector2();
	turn_x = Vector2(1, 0);
	turn_y = Vector2(0, 1);
	if (warp_strength <= 0.0f) {
		return;
	}
	Vector2 p = p_xz - group_offset * WARP_DRIFT;
	for (int i = 0; i < 3; i++) {
		Vector2 k = Vector2(WARP[i].x, WARP[i].y) * WARP_STEP;
		Vector2 across = Vector2(-k.y, k.x).normalized();
		float amplitude = WARP[i].z * warp_strength;
		float phase = k.dot(p) + float(i) * 2.1f;
		push += across * (amplitude * std::cos(phase));
		float gain = -amplitude * std::sin(phase);
		turn_x += k * (across.x * gain);
		turn_y += k * (across.y * gain);
	}
}

Vector3 SeaWaveField::_fetch(const LocalVector<Vector3> &p_grid, float p_u, float p_v) const {
	// The grid holds the field at the points j / n of the tile: no half-texel offset.
	int n = resolution;
	float tx = p_u * n;
	float tz = p_v * n;
	float fx = std::floor(tx);
	float fz = std::floor(tz);
	tx -= fx;
	tz -= fz;
	int x0 = int(fx) % n;
	int z0 = int(fz) % n;
	if (x0 < 0) {
		x0 += n;
	}
	if (z0 < 0) {
		z0 += n;
	}
	int x1 = (x0 + 1) % n;
	int z1 = (z0 + 1) % n;
	const Vector3 &a = p_grid[z0 * n + x0];
	const Vector3 &b = p_grid[z0 * n + x1];
	const Vector3 &c = p_grid[z1 * n + x0];
	const Vector3 &d = p_grid[z1 * n + x1];
	return a.lerp(b, tx).lerp(c.lerp(d, tx), tz);
}

Vector3 SeaWaveField::_fetch_mixed(int p_slot, float p_u, float p_v, bool p_velocity) const {
	const Slot &from = states[mix_from].slots[p_slot];
	const Slot &to = states[mix_to].slots[p_slot];
	Vector3 a;
	if (from.loaded && mix_t < 1.0f) {
		a = _fetch(p_velocity ? from.velocity : from.displacement, p_u, p_v);
	}
	if (mix_from == mix_to || mix_t <= 0.0f) {
		return a;
	}
	Vector3 b;
	if (to.loaded) {
		b = _fetch(p_velocity ? to.velocity : to.displacement, p_u, p_v);
	}
	return a.lerp(b, mix_t);
}

Vector3 SeaWaveField::_sum(const Vector2 &p_point, bool p_velocity) const {
	Vector3 d;
	if (states.is_empty()) {
		return d;
	}
	Vector2 at = p_point + push;
	for (uint32_t s = 0; s < cascades.size(); s++) {
		float gain = gains[s];
		if (gain <= 0.001f) {
			continue;
		}
		const Cascade &cascade = cascades[s];
		int c = CLAMP(cascade.id, 0, 3);
		float inv = 1.0f / cascade.length;
		float share = shares[s];
		if (share < 1.0f) {
			d += _fetch_mixed(s, at.x * inv, at.y * inv, p_velocity) * (gain * std::sqrt(1.0f - share));
		}
		if (share > 0.0f) {
			const Vector2 &t = WEAVE_TURN[c];
			Vector2 reading = Vector2(t.x * at.x - t.y * at.y, t.y * at.x + t.x * at.y) * inv + WEAVE_SHIFT[c];
			Vector3 other = _fetch_mixed(s, reading.x, reading.y, p_velocity);
			Vector2 sideways = Vector2(t.x * other.x + t.y * other.z, t.x * other.z - t.y * other.x);
			d += Vector3(sideways.x, other.y, sideways.y) * (gain * std::sqrt(share));
		}
	}
	return d;
}

Vector3 SeaWaveField::_displacement(const Vector2 &p_point, bool p_velocity) const {
	Vector3 d = _sum(p_point, p_velocity) * scale;
	Vector2 side = turned(Vector2(d.x, d.z));
	return Vector3(side.x, d.y, side.y);
}

Vector3 SeaWaveField::sea(const Vector2 &p_point) const {
	return _sum(p_point, false);
}

Vector3 SeaWaveField::sea_velocity(const Vector2 &p_point) const {
	return _sum(p_point, true);
}

Vector2 SeaWaveField::turned(const Vector2 &p_vector) const {
	return turn_x * p_vector.x + turn_y * p_vector.y;
}

Vector2 SeaWaveField::get_push() const {
	return push;
}

Vector4 SeaWaveField::surface(const Vector2 &p_xz, int p_iterations) {
	set_warp(p_xz);
	Vector2 p = p_xz;
	for (int i = 0; i < p_iterations; i++) {
		Vector3 d = _displacement(p, false);
		p = p_xz - Vector2(d.x, d.z);
	}
	Vector3 d = _displacement(p, false);
	Vector3 v = _displacement(p, true);
	last_grid_point = p;
	return Vector4(v.x, v.y, v.z, d.y);
}

Vector2 SeaWaveField::get_last_grid_point() const {
	return last_grid_point;
}

PackedVector4Array SeaWaveField::surfaces(const PackedVector2Array &p_points, int p_iterations) {
	PackedVector4Array out;
	out.resize(p_points.size());
	batch_grid_points.resize(p_points.size());
	Vector4 *w = out.ptrw();
	Vector2 *g = batch_grid_points.ptrw();
	for (int i = 0; i < p_points.size(); i++) {
		w[i] = surface(p_points[i], p_iterations);
		g[i] = last_grid_point;
	}
	return out;
}

PackedVector2Array SeaWaveField::get_batch_grid_points() const {
	return batch_grid_points;
}

PackedVector3Array SeaWaveField::get_grid(int p_state, int p_slot, bool p_velocity) const {
	PackedVector3Array out;
	ERR_FAIL_INDEX_V(p_state, int(states.size()), out);
	ERR_FAIL_INDEX_V(p_slot, int(cascades.size()), out);
	const Slot &slot = states[p_state].slots[p_slot];
	const LocalVector<Vector3> &grid = p_velocity ? slot.velocity : slot.displacement;
	out.resize(grid.size());
	for (uint32_t i = 0; i < grid.size(); i++) {
		out.set(i, grid[i]);
	}
	return out;
}

void SeaWaveField::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "lengths", "ids", "time_offsets", "resolution", "seed", "period", "gravity", "dispersion_depth"), &SeaWaveField::setup);
	ClassDB::bind_method(D_METHOD("set_state_count", "count"), &SeaWaveField::set_state_count);
	ClassDB::bind_method(D_METHOD("get_state_count"), &SeaWaveField::get_state_count);
	ClassDB::bind_method(D_METHOD("set_spectrum", "state", "slot", "params"), &SeaWaveField::set_spectrum);
	ClassDB::bind_method(D_METHOD("set_gains", "state", "slot", "height_gain", "side_gain"), &SeaWaveField::set_gains);
	ClassDB::bind_method(D_METHOD("is_loaded", "state"), &SeaWaveField::is_loaded);
	ClassDB::bind_method(D_METHOD("update", "time"), &SeaWaveField::update);
	ClassDB::bind_method(D_METHOD("set_mix", "from", "to", "t"), &SeaWaveField::set_mix);
	ClassDB::bind_method(D_METHOD("set_context", "gains", "shares", "scale"), &SeaWaveField::set_context);
	ClassDB::bind_method(D_METHOD("set_warp_params", "group_offset", "strength"), &SeaWaveField::set_warp_params);
	ClassDB::bind_method(D_METHOD("set_warp", "xz"), &SeaWaveField::set_warp);
	ClassDB::bind_method(D_METHOD("sea", "point"), &SeaWaveField::sea);
	ClassDB::bind_method(D_METHOD("sea_velocity", "point"), &SeaWaveField::sea_velocity);
	ClassDB::bind_method(D_METHOD("turned", "vector"), &SeaWaveField::turned);
	ClassDB::bind_method(D_METHOD("get_push"), &SeaWaveField::get_push);
	ClassDB::bind_method(D_METHOD("surface", "xz", "iterations"), &SeaWaveField::surface, DEFVAL(2));
	ClassDB::bind_method(D_METHOD("get_last_grid_point"), &SeaWaveField::get_last_grid_point);
	ClassDB::bind_method(D_METHOD("surfaces", "points", "iterations"), &SeaWaveField::surfaces, DEFVAL(2));
	ClassDB::bind_method(D_METHOD("get_batch_grid_points"), &SeaWaveField::get_batch_grid_points);
	ClassDB::bind_method(D_METHOD("get_grid", "state", "slot", "velocity"), &SeaWaveField::get_grid);
	ClassDB::bind_static_method("SeaWaveField", D_METHOD("gaussian_pair", "mx", "mz", "cascade", "seed"), &SeaWaveField::gaussian_pair);
}
