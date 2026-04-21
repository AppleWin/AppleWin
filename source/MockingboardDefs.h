#pragma once

// PHASOR_MODE: Circular dependency for Mockingboard.h & SSI263.h - so put it here for now
enum PHASOR_MODE { PH_Mockingboard = 0, PH_UNDEF1, PH_UNDEF2, PH_UNDEF3, PH_UNDEF4, PH_Phasor/*=5*/, PH_UNDEF6, PH_EchoPlus/*=7*/ };

const UINT NUM_SY6522 = 2;
const UINT NUM_AY8913 = 4;	// Phasor has 4, MB has 2
const UINT NUM_SSI263 = 2;

const UINT NUM_SUBUNITS_PER_MB = NUM_SY6522;
const UINT NUM_AY8913_PER_SUBUNIT = NUM_AY8913 / NUM_SUBUNITS_PER_MB;
const UINT NUM_VOICES_PER_AY8913 = 3;
const UINT NUM_VOICES = NUM_AY8913 * NUM_VOICES_PER_AY8913;
