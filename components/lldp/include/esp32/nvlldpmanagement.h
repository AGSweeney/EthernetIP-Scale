/*
 * Copyright (c) 2025, Adam G. Sweeney <agsweeney@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef NVLLDPMANAGEMENT_H_
#define NVLLDPMANAGEMENT_H_

#include "ciptypes.h"
#include "typedefs.h"
#include "ciplldpmanagement.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load LLDP Management Object configuration from non-volatile storage
 * 
 * @param values Pointer to structure to fill with loaded values
 * @return kEipStatusOk on success, kEipStatusError on failure (use defaults)
 */
EipStatus NvLldpManagementLoad(CipLldpManagementObjectValues *values);

/**
 * @brief Store LLDP Management Object configuration to non-volatile storage
 * 
 * @param values Pointer to structure containing values to save
 * @return kEipStatusOk on success, kEipStatusError on failure
 */
EipStatus NvLldpManagementStore(const CipLldpManagementObjectValues *values);

#ifdef __cplusplus
}
#endif

#endif /* NVLLDPMANAGEMENT_H_ */

