/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
****************************************************************************/

#include "qplatformdefs.h"

#include "qsharedmemory.h"
#include "qsharedmemory_p.h"
#include "qsystemsemaphore.h"
#include <qdebug.h>

#include <errno.h>

#ifndef QT_NO_SHAREDMEMORY
#include <sys/types.h>
#ifndef QT_POSIX_IPC
#include <sys/ipc.h>
#include <sys/shm.h>
#else
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif //QT_NO_SHAREDMEMORY

#include "private/qcore_unix_p.h"

#ifndef QT_NO_SHAREDMEMORY
QT_BEGIN_NAMESPACE

QSharedMemoryPrivate::QSharedMemoryPrivate() :
#ifndef QT_NO_QOBJECT
    QObjectPrivate(),
#endif
    memory(nullptr), size(0), error(QSharedMemory::NoError),
#ifndef QT_NO_SYSTEMSEMAPHORE
      systemSemaphore(QString()), lockedByMe(false),
#endif
#ifndef QT_POSIX_IPC
      unix_key(0)
#else
      hand(-1)
#endif
{
}

void QSharedMemoryPrivate::setErrorString(QLatin1String function)
{
    // EINVAL is handled in functions so they can give better error strings
    switch (errno) {
    case EACCES:
        errorString = QSharedMemory::tr("%1: permission denied").arg(function);
        error = QSharedMemory::PermissionDenied;
        break;
    case EEXIST:
        errorString = QSharedMemory::tr("%1: already exists").arg(function);
        error = QSharedMemory::AlreadyExists;
        break;
    case ENOENT:
        errorString = QSharedMemory::tr("%1: doesn't exist").arg(function);
        error = QSharedMemory::NotFound;
        break;
    case EMFILE:
    case ENOMEM:
    case ENOSPC:
        errorString = QSharedMemory::tr("%1: out of resources").arg(function);
        error = QSharedMemory::OutOfResources;
        break;
    default:
        errorString = QSharedMemory::tr("%1: unknown error %2").arg(function).arg(errno);
        error = QSharedMemory::UnknownError;
#if defined QSHAREDMEMORY_DEBUG
        qDebug() << errorString << "key" << key << "errno" << errno << EINVAL;
#endif
    }
}

QT_END_NAMESPACE

#endif // QT_NO_SHAREDMEMORY
