/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/light/common.h"

CCL_NAMESPACE_BEGIN

/* Simple CDF based sampling over all lights in the scene, without taking into
 * account shading position or normal. */

ccl_device int light_distribution_sample(KernelGlobals kg, const float rand)
{
  /* This is basically std::upper_bound as used by PBRT, to find a point light or
   * triangle to emit from, proportional to area. a good improvement would be to
   * also sample proportional to power, though it's not so well defined with
   * arbitrary shaders.
   *
   * The original implementation branches on `rand < totarea` each step. The
   * comparison is essentially 50/50 by construction (it splits the search
   * range in half), so every branch mispredicts about half the time. Modern
   * x86 pays ~15 cycles per mispredict, which dominates the inner-loop cost
   * once the table fits in cache. We restructure the loop so the comparison
   * is materialised as a 0/1 integer that biases `first` and `len`
   * arithmetically; MSVC/clang emit cmov for the conditional updates and the
   * loop is then branchless. */
  int first = 0;
  int len = kernel_data.integrator.num_distribution + 1;

  do {
    const int half_len = len >> 1;
    const int middle = first + half_len;
    const float v = kernel_data_fetch(light_distribution, middle).totarea;

    /* go_right = 1 when we should keep the right half, 0 otherwise. */
    const int go_right = (rand >= v) ? 1 : 0;
    /* If go_right: first = middle + 1, len = len - half_len - 1.
     * Else        : first stays,       len = half_len. */
    first += go_right * (half_len + 1);
    len = go_right ? (len - half_len - 1) : half_len;
  } while (len > 0);

  /* Clamping should not be needed but float rounding errors seem to
   * make this fail on rare occasions. */
  const int index = clamp(first - 1, 0, kernel_data.integrator.num_distribution - 1);

  return index;
}

ccl_device_noinline bool light_distribution_sample(KernelGlobals kg,
                                                   const float rand,
                                                   ccl_private LightSample *ls)
{
  /* Sample light index from distribution. */
  ls->emitter_id = light_distribution_sample(kg, rand);
  ls->pdf_selection = kernel_data.integrator.distribution_pdf_lights;
  return true;
}

ccl_device_inline float light_distribution_pdf_lamp(KernelGlobals kg)
{
  return kernel_data.integrator.distribution_pdf_lights;
}

CCL_NAMESPACE_END
