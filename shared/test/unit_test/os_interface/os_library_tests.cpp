/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <memory>

namespace Os {
extern const char *testDllName;
} // namespace Os
const std::string fakeLibName = "_fake_library_name_";
const std::string fnName = "testDynamicLibraryFunc";

using namespace NEO;

TEST(OSLibraryTest, whenLibraryNameIsEmptyThenCurrentProcesIsUsedAsLibrary) {
    std::unique_ptr<OsLibrary> library{OsLibrary::loadFunc({""})};
    EXPECT_NE(nullptr, library);
    void *ptr = library->getProcAddress("selfDynamicLibraryFunc");
    EXPECT_NE(nullptr, ptr);
}

TEST(OSLibraryTest, GivenFakeLibNameWhenLoadingLibraryThenNullIsReturned) {
    OsLibrary *library = OsLibrary::loadFunc(fakeLibName);
    EXPECT_EQ(nullptr, library);
}

TEST(OSLibraryTest, GivenFakeLibNameWhenLoadingLibraryThenNullIsReturnedAndErrorString) {
    std::string errorValue;
    OsLibraryCreateProperties properties(fakeLibName);
    properties.errorValue = &errorValue;
    OsLibrary *library = OsLibrary::loadFunc(properties);
    EXPECT_FALSE(errorValue.empty());
    EXPECT_EQ(nullptr, library);
}

TEST(OSLibraryTest, GivenValidLibNameWhenLoadingLibraryThenLibraryIsLoaded) {
    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc({Os::testDllName}));
    EXPECT_NE(nullptr, library);
}

TEST(OSLibraryTest, GivenValidLibNameWhenLoadingLibraryThenLibraryIsLoadedWithNoErrorString) {
    std::string errorValue;
    OsLibraryCreateProperties properties(Os::testDllName);
    properties.errorValue = &errorValue;
    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc(properties));
    EXPECT_TRUE(errorValue.empty());
    EXPECT_NE(nullptr, library);
}

TEST(OSLibraryTest, whenSymbolNameIsValidThenGetProcAddressReturnsNonNullPointer) {
    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc({Os::testDllName}));
    EXPECT_NE(nullptr, library);
    void *ptr = library->getProcAddress(fnName);
    EXPECT_NE(nullptr, ptr);
}

TEST(OSLibraryTest, whenSymbolNameIsInvalidThenGetProcAddressReturnsNullPointer) {
    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc({Os::testDllName}));
    EXPECT_NE(nullptr, library);
    void *ptr = library->getProcAddress(fnName + "invalid");
    EXPECT_EQ(nullptr, ptr);
}

using OsLibraryTestWithFailureInjection = Test<MemoryManagementFixture>;

TEST_F(OsLibraryTestWithFailureInjection, GivenFailureInjectionWhenLibraryIsLoadedThenOnlyFailedAllocationIsNull) {
    InjectedFunction method = [](size_t failureIndex) {
        std::string libName(Os::testDllName);

        // System under test
        OsLibrary *library = OsLibrary::loadFunc(libName);

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_NE(nullptr, library);
        } else {
            EXPECT_EQ(nullptr, library);
        }

        // Make sure that we only have 1 buffer allocated at a time
        delete library;
    };
    injectFailures(method);
}

TEST(OsLibrary, whenCallingIndexOperatorThenObjectConvertibleToFunctionOrVoidPointerIsReturned) {
    struct MockOsLibrary : OsLibrary {
        void *getProcAddress(const std::string &procName) override {
            lastRequestedProcName = procName;
            return ptrToReturn;
        }
        bool isLoaded() override { return true; }
        std::string getFullPath() override { return std::string(); }

        void *ptrToReturn = nullptr;
        std::string lastRequestedProcName;
    };

    MockOsLibrary lib;

    int varA;
    int varB;
    int varC;

    using FunctionTypeA = void (*)(int *, float);
    using FunctionTypeB = int (*)();

    lib.ptrToReturn = &varA;
    FunctionTypeA functionA = lib["funcA"];
    EXPECT_STREQ("funcA", lib.lastRequestedProcName.c_str());
    EXPECT_EQ(&varA, reinterpret_cast<void *>(functionA));

    lib.ptrToReturn = &varB;
    FunctionTypeB functionB = lib["funcB"];
    EXPECT_STREQ("funcB", lib.lastRequestedProcName.c_str());
    EXPECT_EQ(&varB, reinterpret_cast<void *>(functionB));

    lib.ptrToReturn = &varC;
    void *rawPtr = lib["funcC"];
    EXPECT_STREQ("funcC", lib.lastRequestedProcName.c_str());
    EXPECT_EQ(&varC, rawPtr);
}
