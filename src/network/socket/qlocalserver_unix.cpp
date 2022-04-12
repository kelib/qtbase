/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qlocalserver.h"
#include "qlocalserver_p.h"
#include "qlocalsocket.h"
#include "qlocalsocket_p.h"
#include "qnet_unix_p.h"
#include "qtemporarydir.h"

#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <qdebug.h>
#include <qdir.h>
#include <qdatetime.h>

#include <optional>

#ifdef Q_OS_VXWORKS
#  include <selectLib.h>
#endif

QT_BEGIN_NAMESPACE

namespace {
QLocalServer::SocketOptions optionsForPlatform(QLocalServer::SocketOptions srcOptions)
{
    // For OS that does not support abstract namespace the AbstractNamespaceOption
    // means that we go for WorldAccessOption - as it is the closest option in
    // regards of access rights. In Linux/Android case we clean-up the access rights.

    if (srcOptions.testFlag(QLocalServer::AbstractNamespaceOption)) {
        if (PlatformSupportsAbstractNamespace)
            return QLocalServer::AbstractNamespaceOption;
        else
            return QLocalServer::WorldAccessOption;
    }
    return srcOptions;
}
}

void QLocalServerPrivate::init()
{
}

bool QLocalServerPrivate::removeServer(const QString &name)
{
    QString fileName;
    if (name.startsWith(u'/')) {
        fileName = name;
    } else {
        fileName = QDir::cleanPath(QDir::tempPath());
        fileName += u'/' + name;
    }
    if (QFile::exists(fileName))
        return QFile::remove(fileName);
    else
        return true;
}

bool QLocalServerPrivate::listen(const QString &requestedServerName)
{
    Q_Q(QLocalServer);

    // socket options adjusted for current platform
    auto options = optionsForPlatform(socketOptions.value());

    // determine the full server path
    if (options.testFlag(QLocalServer::AbstractNamespaceOption)
        ||  requestedServerName.startsWith(u'/')) {
        fullServerName = requestedServerName;
    } else {
        fullServerName = QDir::cleanPath(QDir::tempPath());
        fullServerName += u'/' + requestedServerName;
    }
    serverName = requestedServerName;

    QByteArray encodedTempPath;
    const QByteArray encodedFullServerName = QFile::encodeName(fullServerName);
    std::optional<QTemporaryDir> tempDir;

    if (options & QLocalServer::WorldAccessOption) {
        QFileInfo serverNameFileInfo(fullServerName);
        tempDir.emplace(serverNameFileInfo.absolutePath() + u'/');
        if (!tempDir->isValid()) {
            setError(QLatin1String("QLocalServer::listen"));
            return false;
        }
        encodedTempPath = QFile::encodeName(tempDir->path() + QLatin1String("/s"));
    }

    // create the unix socket
    listenSocket = qt_safe_socket(PF_UNIX, SOCK_STREAM, 0);
    if (-1 == listenSocket) {
        setError(QLatin1String("QLocalServer::listen"));
        closeServer();
        return false;
    }

    // Construct the unix address
    struct ::sockaddr_un addr;

    addr.sun_family = PF_UNIX;
    ::memset(addr.sun_path, 0, sizeof(addr.sun_path));

    // for abstract namespace add 2 to length, to take into account trailing AND leading null
    constexpr unsigned int extraCharacters = PlatformSupportsAbstractNamespace ? 2 : 1;

    if (sizeof(addr.sun_path) < static_cast<size_t>(encodedFullServerName.size() + extraCharacters)) {
        setError(QLatin1String("QLocalServer::listen"));
        closeServer();
        return false;
    }

    QT_SOCKLEN_T addrSize = sizeof(::sockaddr_un);
    if (options.testFlag(QLocalServer::AbstractNamespaceOption)) {
        // Abstract socket address is distinguished by the fact
        // that sun_path[0] is a null byte ('\0')
        ::memcpy(addr.sun_path + 1, encodedFullServerName.constData(),
                 encodedFullServerName.size() + 1);
        addrSize = offsetof(::sockaddr_un, sun_path) + encodedFullServerName.size() + 1;
    } else if (options & QLocalServer::WorldAccessOption) {
        if (sizeof(addr.sun_path) < static_cast<size_t>(encodedTempPath.size() + 1)) {
            setError(QLatin1String("QLocalServer::listen"));
            closeServer();
            return false;
        }
        ::memcpy(addr.sun_path, encodedTempPath.constData(),
                 encodedTempPath.size() + 1);
    } else {
        ::memcpy(addr.sun_path, encodedFullServerName.constData(),
                 encodedFullServerName.size() + 1);
    }

    // bind
    if (-1 == QT_SOCKET_BIND(listenSocket, (sockaddr *)&addr, addrSize)) {
        setError(QLatin1String("QLocalServer::listen"));
        // if address is in use already, just close the socket, but do not delete the file
        if (errno == EADDRINUSE)
            QT_CLOSE(listenSocket);
        // otherwise, close the socket and delete the file
        else
            closeServer();
        listenSocket = -1;
        return false;
    }

    // listen for connections
    if (-1 == qt_safe_listen(listenSocket, listenBacklog)) {
        setError(QLatin1String("QLocalServer::listen"));
        closeServer();
        return false;
    }

    if (options & QLocalServer::WorldAccessOption) {
        mode_t mode = 000;

        if (options & QLocalServer::UserAccessOption)
            mode |= S_IRWXU;

        if (options & QLocalServer::GroupAccessOption)
            mode |= S_IRWXG;

        if (options & QLocalServer::OtherAccessOption)
            mode |= S_IRWXO;

        if (::chmod(encodedTempPath.constData(), mode) == -1) {
            setError(QLatin1String("QLocalServer::listen"));
            closeServer();
            return false;
        }

        if (::rename(encodedTempPath.constData(), encodedFullServerName.constData()) == -1) {
            setError(QLatin1String("QLocalServer::listen"));
            closeServer();
            return false;
        }
    }

    Q_ASSERT(!socketNotifier);
    socketNotifier = new QSocketNotifier(listenSocket,
                                         QSocketNotifier::Read, q);
    q->connect(socketNotifier, SIGNAL(activated(QSocketDescriptor)),
               q, SLOT(_q_onNewConnection()));
    socketNotifier->setEnabled(maxPendingConnections > 0);
    return true;
}

bool QLocalServerPrivate::listen(qintptr socketDescriptor)
{
    Q_Q(QLocalServer);

    // Attach to the localsocket
    listenSocket = socketDescriptor;

    ::fcntl(listenSocket, F_SETFD, FD_CLOEXEC);
    ::fcntl(listenSocket, F_SETFL, ::fcntl(listenSocket, F_GETFL) | O_NONBLOCK);

    bool abstractAddress = false;
    struct ::sockaddr_un addr;
    QT_SOCKLEN_T len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    if (::getsockname(socketDescriptor, (sockaddr *)&addr, &len) == 0) {
#if defined(Q_OS_QNX)
        if (addr.sun_path[0] == 0 && addr.sun_path[1] == 0)
            len = SUN_LEN(&addr);
#endif
        if (QLocalSocketPrivate::parseSockaddr(addr, len, fullServerName, serverName,
                                               abstractAddress)) {
            QLocalServer::SocketOptions options = socketOptions.value();
            socketOptions = options.setFlag(QLocalServer::AbstractNamespaceOption, abstractAddress);
        }
    }

    Q_ASSERT(!socketNotifier);
    socketNotifier = new QSocketNotifier(listenSocket,
                                         QSocketNotifier::Read, q);
    q->connect(socketNotifier, SIGNAL(activated(QSocketDescriptor)),
               q, SLOT(_q_onNewConnection()));
    socketNotifier->setEnabled(maxPendingConnections > 0);
    return true;
}

/*!
    \internal

    \sa QLocalServer::closeServer()
 */
void QLocalServerPrivate::closeServer()
{
    if (socketNotifier) {
        socketNotifier->setEnabled(false); // Otherwise, closed socket is checked before deleter runs
        socketNotifier->deleteLater();
        socketNotifier = nullptr;
    }

    if (-1 != listenSocket)
        QT_CLOSE(listenSocket);
    listenSocket = -1;

    if (!fullServerName.isEmpty()
        && !optionsForPlatform(socketOptions).testFlag(QLocalServer::AbstractNamespaceOption)) {
        QFile::remove(fullServerName);
    }

    serverName.clear();
    fullServerName.clear();
}

/*!
    \internal

    We have received a notification that we can read on the listen socket.
    Accept the new socket.
 */
void QLocalServerPrivate::_q_onNewConnection()
{
    Q_Q(QLocalServer);
    if (-1 == listenSocket)
        return;

    ::sockaddr_un addr;
    QT_SOCKLEN_T length = sizeof(sockaddr_un);
    int connectedSocket = qt_safe_accept(listenSocket, (sockaddr *)&addr, &length);
    if (-1 == connectedSocket) {
        setError(QLatin1String("QLocalSocket::activated"));
        closeServer();
    } else {
        socketNotifier->setEnabled(pendingConnections.size()
                                   <= maxPendingConnections);
        q->incomingConnection(connectedSocket);
    }
}

void QLocalServerPrivate::waitForNewConnection(int msec, bool *timedOut)
{
    pollfd pfd = qt_make_pollfd(listenSocket, POLLIN);

    switch (qt_poll_msecs(&pfd, 1, msec)) {
    case 0:
        if (timedOut)
            *timedOut = true;

        return;
        break;
    default:
        if ((pfd.revents & POLLNVAL) == 0) {
            _q_onNewConnection();
            return;
        }

        errno = EBADF;
        Q_FALLTHROUGH();
    case -1:
        setError(QLatin1String("QLocalServer::waitForNewConnection"));
        closeServer();
        break;
    }
}

void QLocalServerPrivate::setError(const QString &function)
{
    if (EAGAIN == errno)
        return;

    switch (errno) {
    case EACCES:
        errorString = QLocalServer::tr("%1: Permission denied").arg(function);
        error = QAbstractSocket::SocketAccessError;
        break;
    case ELOOP:
    case ENOENT:
    case ENAMETOOLONG:
    case EROFS:
    case ENOTDIR:
        errorString = QLocalServer::tr("%1: Name error").arg(function);
        error = QAbstractSocket::HostNotFoundError;
        break;
    case EADDRINUSE:
        errorString = QLocalServer::tr("%1: Address in use").arg(function);
        error = QAbstractSocket::AddressInUseError;
        break;

    default:
        errorString = QLocalServer::tr("%1: Unknown error %2")
                      .arg(function).arg(errno);
        error = QAbstractSocket::UnknownSocketError;
#if defined QLOCALSERVER_DEBUG
        qWarning() << errorString << "fullServerName:" << fullServerName;
#endif
    }
}

QT_END_NAMESPACE
