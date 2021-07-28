/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtTest module of the Qt Toolkit.
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

#include <QtTest/private/qjunittestlogger_p.h>
#include <QtTest/private/qtestelement_p.h>
#include <QtTest/private/qtestjunitstreamer_p.h>
#include <QtTest/qtestcase.h>
#include <QtTest/private/qtestresult_p.h>
#include <QtTest/private/qbenchmark_p.h>
#include <QtTest/private/qtestlog_p.h>

#ifdef min // windows.h without NOMINMAX is included by the benchmark headers.
#  undef min
#endif
#ifdef max
#  undef max
#endif

#include <QtCore/qlibraryinfo.h>

#include <string.h>

QT_BEGIN_NAMESPACE

QJUnitTestLogger::QJUnitTestLogger(const char *filename)
    : QAbstractTestLogger(filename)
{
}

QJUnitTestLogger::~QJUnitTestLogger()
{
    Q_ASSERT(!currentTestSuite);
    delete logFormatter;
}

void QJUnitTestLogger::startLogging()
{
    QAbstractTestLogger::startLogging();

    logFormatter = new QTestJUnitStreamer(this);
    delete systemOutputElement;
    systemOutputElement = new QTestElement(QTest::LET_SystemOutput);
    delete systemErrorElement;
    systemErrorElement = new QTestElement(QTest::LET_SystemError);

    Q_ASSERT(!currentTestSuite);
    currentTestSuite = new QTestElement(QTest::LET_TestSuite);
    currentTestSuite->addAttribute(QTest::AI_Name, QTestResult::currentTestObjectName());

    auto localTime = QDateTime::currentDateTime();
    auto localTimeWithUtcOffset = localTime.toOffsetFromUtc(localTime.offsetFromUtc());
    currentTestSuite->addAttribute(QTest::AI_Timestamp,
        localTimeWithUtcOffset.toString(Qt::ISODate).toUtf8().constData());

    QTestElement *property;
    QTestElement *properties = new QTestElement(QTest::LET_Properties);

    property = new QTestElement(QTest::LET_Property);
    property->addAttribute(QTest::AI_Name, "QTestVersion");
    property->addAttribute(QTest::AI_PropertyValue, QTEST_VERSION_STR);
    properties->addLogElement(property);

    property = new QTestElement(QTest::LET_Property);
    property->addAttribute(QTest::AI_Name, "QtVersion");
    property->addAttribute(QTest::AI_PropertyValue, qVersion());
    properties->addLogElement(property);

    property = new QTestElement(QTest::LET_Property);
    property->addAttribute(QTest::AI_Name, "QtBuild");
    property->addAttribute(QTest::AI_PropertyValue, QLibraryInfo::build());
    properties->addLogElement(property);

    currentTestSuite->addLogElement(properties);
}

void QJUnitTestLogger::stopLogging()
{
    char buf[10];

    qsnprintf(buf, sizeof(buf), "%i", testCounter);
    currentTestSuite->addAttribute(QTest::AI_Tests, buf);

    qsnprintf(buf, sizeof(buf), "%i", failureCounter);
    currentTestSuite->addAttribute(QTest::AI_Failures, buf);

    qsnprintf(buf, sizeof(buf), "%i", errorCounter);
    currentTestSuite->addAttribute(QTest::AI_Errors, buf);

    currentTestSuite->addAttribute(QTest::AI_Time,
        QByteArray::number(QTestLog::msecsTotalTime() / 1000, 'f').constData());

    currentTestSuite->addLogElement(listOfTestcases);

    // For correct indenting, make sure every testcase knows its parent
    QTestElement *testcase = listOfTestcases;
    while (testcase) {
        testcase->setParent(currentTestSuite);
        testcase = testcase->nextElement();
    }

    if (systemOutputElement->childElements())
        currentTestSuite->addLogElement(systemOutputElement);
    currentTestSuite->addLogElement(systemErrorElement);

    logFormatter->output(currentTestSuite);

    delete currentTestSuite;
    currentTestSuite = nullptr;

    QAbstractTestLogger::stopLogging();
}

void QJUnitTestLogger::enterTestFunction(const char *function)
{
    currentLogElement = new QTestElement(QTest::LET_TestCase);
    currentLogElement->addAttribute(QTest::AI_Name, function);
    currentLogElement->addToList(&listOfTestcases);

    // The element will be deleted when the suite is deleted

    ++testCounter;
}

void QJUnitTestLogger::leaveTestFunction()
{
    currentLogElement->addAttribute(QTest::AI_Time,
        QByteArray::number(QTestLog::msecsFunctionTime() / 1000, 'f').constData());
}

void QJUnitTestLogger::addIncident(IncidentTypes type, const char *description,
                                   const char *file, int line)
{
    const char *typeBuf = nullptr;

    switch (type) {
    case QAbstractTestLogger::XPass:
        ++failureCounter;
        typeBuf = "xpass";
        break;
    case QAbstractTestLogger::Pass:
        typeBuf = "pass";
        break;
    case QAbstractTestLogger::XFail:
        typeBuf = "xfail";
        break;
    case QAbstractTestLogger::Fail:
        ++failureCounter;
        typeBuf = "fail";
        break;
    case QAbstractTestLogger::BlacklistedPass:
        typeBuf = "bpass";
        break;
    case QAbstractTestLogger::BlacklistedFail:
        ++failureCounter;
        typeBuf = "bfail";
        break;
    case QAbstractTestLogger::BlacklistedXPass:
        typeBuf = "bxpass";
        break;
    case QAbstractTestLogger::BlacklistedXFail:
        ++failureCounter;
        typeBuf = "bxfail";
        break;
    default:
        typeBuf = "??????";
        break;
    }

    if (type == QAbstractTestLogger::Fail || type == QAbstractTestLogger::XPass) {
        QTestElement *failureElement = new QTestElement(QTest::LET_Failure);
        failureElement->addAttribute(QTest::AI_Result, typeBuf);
        failureElement->addAttribute(QTest::AI_Message, description);
        addTag(failureElement);
        currentLogElement->addLogElement(failureElement);
    }

    /*
        Only one result can be shown for the whole testfunction.
        Check if we currently have a result, and if so, overwrite it
        iff the new result is worse.
    */
    QTestElementAttribute* resultAttr =
        const_cast<QTestElementAttribute*>(currentLogElement->attribute(QTest::AI_Result));
    if (resultAttr) {
        const char* oldResult = resultAttr->value();
        bool overwrite = false;
        if (!strcmp(oldResult, "pass")) {
            overwrite = true;
        }
        else if (!strcmp(oldResult, "bpass") || !strcmp(oldResult, "bxfail")) {
            overwrite = (type == QAbstractTestLogger::XPass || type == QAbstractTestLogger::Fail) || (type == QAbstractTestLogger::XFail)
                    || (type == QAbstractTestLogger::BlacklistedFail) || (type == QAbstractTestLogger::BlacklistedXPass);
        }
        else if (!strcmp(oldResult, "bfail") || !strcmp(oldResult, "bxpass")) {
            overwrite = (type == QAbstractTestLogger::XPass || type == QAbstractTestLogger::Fail) || (type == QAbstractTestLogger::XFail);
        }
        else if (!strcmp(oldResult, "xfail")) {
            overwrite = (type == QAbstractTestLogger::XPass || type == QAbstractTestLogger::Fail);
        }
        else if (!strcmp(oldResult, "xpass")) {
            overwrite = (type == QAbstractTestLogger::Fail);
        }
        if (overwrite) {
            resultAttr->setPair(QTest::AI_Result, typeBuf);
        }
    }
    else {
        currentLogElement->addAttribute(QTest::AI_Result, typeBuf);
    }

    /*
        Since XFAIL does not add a failure to the testlog in junitxml, add a message, so we still
        have some information about the expected failure.
    */
    if (type == QAbstractTestLogger::XFail) {
        QJUnitTestLogger::addMessage(QAbstractTestLogger::Info, QString::fromUtf8(description), file, line);
    }
}

void QJUnitTestLogger::addTag(QTestElement* element)
{
    const char *tag = QTestResult::currentDataTag();
    const char *gtag = QTestResult::currentGlobalDataTag();
    const char *filler = (tag && gtag) ? ":" : "";
    if ((!tag || !tag[0]) && (!gtag || !gtag[0])) {
        return;
    }

    if (!tag) {
        tag = "";
    }
    if (!gtag) {
        gtag = "";
    }

    QTestCharBuffer buf;
    QTest::qt_asprintf(&buf, "%s%s%s", gtag, filler, tag);
    element->addAttribute(QTest::AI_Tag, buf.constData());
}

void QJUnitTestLogger::addMessage(MessageTypes type, const QString &message, const char *file, int line)
{
    Q_UNUSED(file);
    Q_UNUSED(line);

    auto messageElement = new QTestElement(QTest::LET_Message);
    auto systemLogElement = systemOutputElement;
    const char *typeBuf = nullptr;

    switch (type) {
    case QAbstractTestLogger::Warn:
        systemLogElement = systemErrorElement;
        typeBuf = "warn";
        break;
    case QAbstractTestLogger::QSystem:
        typeBuf = "system";
        break;
    case QAbstractTestLogger::QDebug:
        typeBuf = "qdebug";
        break;
    case QAbstractTestLogger::QInfo:
        typeBuf = "qinfo";
        break;
    case QAbstractTestLogger::QWarning:
        systemLogElement = systemErrorElement;
        typeBuf = "qwarn";
        break;
    case QAbstractTestLogger::QFatal:
        systemLogElement = systemErrorElement;
        typeBuf = "qfatal";
        break;
    case QAbstractTestLogger::Skip:
        typeBuf = "skip";
        break;
    case QAbstractTestLogger::Info:
        typeBuf = "info";
        break;
    default:
        typeBuf = "??????";
        break;
    }

    messageElement->addAttribute(QTest::AI_Type, typeBuf);
    messageElement->addAttribute(QTest::AI_Message, message.toUtf8().constData());
    addTag(messageElement);

    currentLogElement->addLogElement(messageElement);
    ++errorCounter;

    // Also add the message to the system log (stdout/stderr), if one exists
    if (systemLogElement) {
        auto messageElement = new QTestElement(QTest::LET_Message);
        messageElement->addAttribute(QTest::AI_Message, message.toUtf8().constData());
        systemLogElement->addLogElement(messageElement);
    }
}

QT_END_NAMESPACE

