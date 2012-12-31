/*
 *  Copyright (c) 2012 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __KOVCMULTIARCHBUILDSUPPORT_H
#define __KOVCMULTIARCHBUILDSUPPORT_H

#include "config-vc.h"

#ifdef HAVE_VC

#include <Vc/Vc>
#include <Vc/common/support.h>

#else /* HAVE_VC */

namespace Vc {
    typedef enum {ScalarImpl} Implementation;
}

#define VC_IMPL ::Vc::ScalarImpl

#ifdef DO_PACKAGERS_BUILD
#warning "Packagers build is not available without the presence of Vc library. Disabling."
#undef DO_PACKAGERS_BUILD
#endif

#endif /* HAVE_VC */


#ifdef DO_PACKAGERS_BUILD

template<class FactoryType>
typename FactoryType::ReturnType
createOptimizedClass(typename FactoryType::ParamType param)
{
    /*if (Vc::isImplementationSupported(Vc::Fma4Impl)) {
        return FactoryType::template create<Vc::Fma4Impl>(param);
    } else if (Vc::isImplementationSupported(Vc::XopImpl)) {
        return FactoryType::template create<Vc::XopImpl>(param);
        } else*/
    if (Vc::isImplementationSupported(Vc::AVXImpl)) {
        return FactoryType::template create<Vc::AVXImpl>(param);
    } else if (Vc::isImplementationSupported(Vc::SSE42Impl)) {
        return FactoryType::template create<Vc::SSE42Impl>(param);
    } else if (Vc::isImplementationSupported(Vc::SSE41Impl)) {
        return FactoryType::template create<Vc::SSE41Impl>(param);
    } else if (Vc::isImplementationSupported(Vc::SSE4aImpl)) {
        return FactoryType::template create<Vc::SSE4aImpl>(param);
    } else if (Vc::isImplementationSupported(Vc::SSSE3Impl)) {
        return FactoryType::template create<Vc::SSSE3Impl>(param);
    } else if (Vc::isImplementationSupported(Vc::SSE3Impl)) {
        return FactoryType::template create<Vc::SSE3Impl>(param);
    } else if (Vc::isImplementationSupported(Vc::SSE2Impl)) {
        return FactoryType::template create<Vc::SSE2Impl>(param);
    } else {
        return FactoryType::template create<Vc::ScalarImpl>(param);
    }
}

#else /* DO_PACKAGERS_BUILD */

/**
 * When doing not a packager's build we have one architecture only,
 * so the factory methods are simplified
 */

template<class FactoryType>
typename FactoryType::ReturnType
createOptimizedClass(typename FactoryType::ParamType param)
{
    return FactoryType::template create<VC_IMPL>(param);
}

#endif /* DO_PACKAGERS_BUILD */

#endif /* __KOVCMULTIARCHBUILDSUPPORT_H */