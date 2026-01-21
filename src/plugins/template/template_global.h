#pragma once

#include <QtCore/qglobal.h>

#if defined(TEMPLATE_LIBRARY)
#  define TEMPLATE_EXPORT Q_DECL_EXPORT
#else
#  define TEMPLATE_EXPORT Q_DECL_IMPORT
#endif
