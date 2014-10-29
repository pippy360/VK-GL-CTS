#ifndef _QPINFO_H
#define _QPINFO_H
/*-------------------------------------------------------------------------
 * drawElements Quality Program Helper Library
 * -------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Version and platform info.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

const char*			qpGetTargetName			(void);
const char*			qpGetReleaseName		(void);
deUint32			qpGetReleaseId			(void);

DE_END_EXTERN_C

#endif /* _QPINFO_H */
