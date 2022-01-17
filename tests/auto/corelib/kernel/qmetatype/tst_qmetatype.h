/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore>
#include <QTest>
#include "tst_qmetatype_common.h"
#include "tst_qvariant_common.h"

struct Derived : QObject
{
    Q_OBJECT
};

struct MessageHandlerCustom : public MessageHandler
{
    MessageHandlerCustom(const int typeId)
        : MessageHandler(typeId, handler)
    {}
    static void handler(QtMsgType, const QMessageLogContext &, const QString &msg)
    {
        QCOMPARE(msg.trimmed(), expectedMessage.trimmed());
    }
    inline static QString expectedMessage;
};

class tst_QMetaType: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<QVariant> prop READ prop WRITE setProp)

public:
    struct GadgetPropertyType {
        QByteArray type;
        QByteArray name;
        QVariant testData;
    };

    tst_QMetaType() { propList << 42 << "Hello"; }

    QList<QVariant> prop() const { return propList; }
    void setProp(const QList<QVariant> &list) { propList = list; }

private:
    void registerGadget(const char * name, const QList<GadgetPropertyType> &gadgetProperties);
    QList<QVariant> propList;

private slots:
    void defined();
    void threadSafety();
    void namespaces();
    void id();
    void qMetaTypeId();
    void properties();
    void normalizedTypes();
    void typeName_data();
    void typeName();
    void type_data();
    void type();
    void type_fromSubString_data();
    void type_fromSubString();
    void create_data();
    void create();
    void createCopy_data();
    void createCopy();
    void sizeOf_data();
    void sizeOf();
    void sizeOfStaticLess_data();
    void sizeOfStaticLess();
    void alignOf_data();
    void alignOf();
    void flags_data();
    void flags();
    void flagsStaticLess_data();
    void flagsStaticLess();
    void flagsBinaryCompatibility6_0_data();
    void flagsBinaryCompatibility6_0();
    void construct_data();
    void construct();
    void typedConstruct();
    void constructCopy_data();
    void constructCopy();
    void selfCompare_data();
    void selfCompare();
    void typedefs();
    void registerType();
    void isRegistered_data();
    void isRegistered();
    void isRegisteredStaticLess_data();
    void isRegisteredStaticLess();
    void isEnum();
    void automaticTemplateRegistration_1();
    void automaticTemplateRegistration_2(); // defined in tst_qmetatype3.cpp
    void saveAndLoadBuiltin_data();
    void saveAndLoadBuiltin();
    void saveAndLoadCustom();
    void metaObject_data();
    void metaObject();
    void constexprMetaTypeIds();

    // tst_qmetatype2.cpp
    void constRefs();
    void convertCustomType_data();
    void convertCustomType();
    void convertConstNonConst();
    void compareCustomEqualOnlyType();
    void customDebugStream();
    void unknownType();
    void fromType();
    void operatorEq_data();
    void operatorEq();
    void typesWithInaccessibleDTors();
    void voidIsNotUnknown();
    void typeNameNormalization();
};

template <typename T>
struct Whity { T t; Whity() {} };

Q_DECLARE_METATYPE(Whity<int>)
Q_DECLARE_METATYPE(Whity<double>)

#if !defined(Q_CC_CLANG) && defined(Q_CC_GNU) && Q_CC_GNU < 501
QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(Whity<double>, Q_RELOCATABLE_TYPE);
QT_END_NAMESPACE
#endif

struct CustomConvertibleType
{
    explicit CustomConvertibleType(const QVariant &foo = QVariant()) : m_foo(foo) {}
    virtual ~CustomConvertibleType() {}
    QString toString() const { return m_foo.toString(); }
    operator QPoint() const { return QPoint(12, 34); }
    template<typename To>
    To convert() const { return s_value.value<To>();}
    template<typename To>
    To convertOk(bool *ok) const { *ok = s_ok; return s_value.value<To>();}

    QVariant m_foo;
    inline static QVariant s_value;
    inline static bool s_ok = true;

    friend bool operator<(const CustomConvertibleType &lhs, const CustomConvertibleType &rhs)
    { return lhs.m_foo.toString() < rhs.m_foo.toString(); }
    friend bool operator==(const CustomConvertibleType &lhs, const CustomConvertibleType &rhs)
    { return lhs.m_foo == rhs.m_foo; }
    friend bool operator!=(const CustomConvertibleType &lhs, const CustomConvertibleType &rhs)
    { return !operator==(lhs, rhs); }
};

struct CustomConvertibleType2
{
    // implicit
    CustomConvertibleType2(const CustomConvertibleType &t = CustomConvertibleType())
        : m_foo(t.m_foo) {}
    virtual ~CustomConvertibleType2() {}

    QVariant m_foo;

    friend bool operator==(const CustomConvertibleType2 &lhs, const CustomConvertibleType2 &rhs)
    { return lhs.m_foo == rhs.m_foo; }
    friend bool operator!=(const CustomConvertibleType2 &lhs, const CustomConvertibleType2 &rhs)
    { return !operator==(lhs, rhs); }
};

struct CustomDebugStreamableType
{
    QString toString() const { return "test"; }

    friend QDebug operator<<(QDebug dbg, const CustomDebugStreamableType&)
    {
        return dbg << "string-content";
    }
};

struct CustomDebugStreamableType2
{
    QString toString() const { return "test"; }
};

struct CustomEqualsOnlyType
{
    explicit CustomEqualsOnlyType(int value = 0) : val(value) {}
    virtual ~CustomEqualsOnlyType() {}

    int val;

    friend bool operator==(const CustomEqualsOnlyType &lhs, const CustomEqualsOnlyType &rhs)
    { return lhs.val == rhs.val;}
    friend bool operator!=(const CustomEqualsOnlyType &lhs, const CustomEqualsOnlyType &rhs)
    { return !operator==(lhs, rhs); }
};

static_assert(QTypeTraits::has_operator_equal_v<CustomEqualsOnlyType>);
static_assert(!QTypeTraits::has_operator_less_than_v<CustomEqualsOnlyType>);

struct BaseGadgetType
{
    Q_GADGET
public:
    explicit BaseGadgetType(QVariant foo = QVariant()) : m_foo(std::move(foo)) {}
    QVariant m_foo;
};

struct DerivedGadgetType : public BaseGadgetType
{
    Q_GADGET
public:
    explicit DerivedGadgetType(QVariant foo = QVariant()) : BaseGadgetType(std::move(foo)) {}
    int bar = 25;
};

Q_DECLARE_METATYPE(CustomConvertibleType);
Q_DECLARE_METATYPE(CustomConvertibleType2);
Q_DECLARE_METATYPE(CustomDebugStreamableType);
Q_DECLARE_METATYPE(CustomEqualsOnlyType);

struct CustomMovable {
    CustomMovable() {}

    friend bool operator==(const CustomMovable &, const CustomMovable &) { return true; }
    // needed for QSet<CustomMovable>. We actually check that it makes sense.
    friend qsizetype qHash(const CustomMovable &, qsizetype seed = 0) { return seed; }
};

#if !defined(Q_CC_CLANG) && defined(Q_CC_GNU) && Q_CC_GNU < 501
QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(CustomMovable, Q_RELOCATABLE_TYPE);
QT_END_NAMESPACE
#endif

Q_DECLARE_METATYPE(CustomMovable);

#define FOR_EACH_STATIC_PRIMITIVE_TYPE(F) \
    F(bool) \
    F(int) \
    F(qulonglong) \
    F(double) \
    F(short) \
    F(char) \
    F(ulong) \
    F(uchar) \
    F(float) \
    F(QObject*) \
    F(QString) \
    F(CustomMovable)

#define FOR_EACH_STATIC_PRIMITIVE_TYPE2(F, SecondaryRealName) \
    F(uint, SecondaryRealName) \
    F(qlonglong, SecondaryRealName) \
    F(char, SecondaryRealName) \
    F(uchar, SecondaryRealName) \
    F(QObject*, SecondaryRealName)

#define CREATE_AND_VERIFY_CONTAINER(CONTAINER, ...) \
    { \
        CONTAINER< __VA_ARGS__ > t; \
        const QVariant v = QVariant::fromValue(t); \
        QByteArray tn = createTypeName(#CONTAINER "<", #__VA_ARGS__); \
        const int expectedType = ::qMetaTypeId<CONTAINER< __VA_ARGS__ > >(); \
        const int type = QMetaType::type(tn); \
        QCOMPARE(type, expectedType); \
        QCOMPARE((QMetaType::fromType<CONTAINER< __VA_ARGS__ >>().id()), expectedType); \
    }

#define FOR_EACH_1ARG_TEMPLATE_TYPE(F, TYPE) \
    F(QList, TYPE) \
    F(QQueue, TYPE) \
    F(QStack, TYPE) \
    F(QSet, TYPE)

#define PRINT_1ARG_TEMPLATE(RealName) \
    FOR_EACH_1ARG_TEMPLATE_TYPE(CREATE_AND_VERIFY_CONTAINER, RealName)

#define FOR_EACH_2ARG_TEMPLATE_TYPE(F, RealName1, RealName2) \
    F(QHash, RealName1, RealName2) \
    F(QMap, RealName1, RealName2) \
    F(std::pair, RealName1, RealName2)

#define PRINT_2ARG_TEMPLATE_INTERNAL(RealName1, RealName2) \
    FOR_EACH_2ARG_TEMPLATE_TYPE(CREATE_AND_VERIFY_CONTAINER, RealName1, RealName2)

#define PRINT_2ARG_TEMPLATE(RealName) \
    FOR_EACH_STATIC_PRIMITIVE_TYPE2(PRINT_2ARG_TEMPLATE_INTERNAL, RealName)

#define REGISTER_TYPEDEF(TYPE, ARG1, ARG2) \
  qRegisterMetaType<TYPE <ARG1, ARG2>>(#TYPE "<" #ARG1 "," #ARG2 ">");

static inline QByteArray createTypeName(const char *begin, const char *va)
{
    QByteArray tn(begin);
    const QList<QByteArray> args = QByteArray(va).split(',');
    tn += args.first().trimmed();
    if (args.size() > 1) {
        QList<QByteArray>::const_iterator it = args.constBegin() + 1;
        const QList<QByteArray>::const_iterator end = args.constEnd();
        for (; it != end; ++it) {
            tn += ",";
            tn += it->trimmed();
        }
    }
    if (tn.endsWith('>'))
        tn += ' ';
    tn += '>';
    return tn;
}

Q_DECLARE_METATYPE(const void*)
