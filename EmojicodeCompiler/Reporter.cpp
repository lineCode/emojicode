//
//  Reporter.c
//  Emojicode
//
//  Created by Theo Weidmann on 06.11.15.
//  Copyright © 2015 Theo Weidmann. All rights reserved.
//

#include "EmojicodeCompiler.hpp"
#include "utf8.h"
#include "Lexer.hpp"
#include "Procedure.hpp"
#include "Class.hpp"
#include <cstring>

enum ReturnManner {
    Return,
    NoReturn,
    CanReturnNothingness
};

void reportDocumentation(const Token *documentationToken) {
    if(!documentationToken) {
        return;
    }
    
    const char *d = documentationToken->value.utf8CString();
    printf("\"documentation\":");
    printJSONStringToFile(d, stdout);
    putc(',', stdout);
    delete [] d;
}

void reportType(const char *key, Type type, bool last, TypeContext tc){
    auto returnTypeName = type.toString(tc, false).c_str();
    
    if (key) {
        printf("\"%s\": {\"package\": \"%s\", \"name\": \"%s\", \"optional\": %s}%s", key, type.typePackage(), returnTypeName, type.optional ? "true" : "false", last ? "" : ",");
    }
    else {
        printf("{\"package\": \"%s\", \"name\": \"%s\", \"optional\": %s}%s", type.typePackage(), returnTypeName, type.optional ? "true" : "false", last ? "" : ",");
    }
}

void reportGenericArguments(std::map<EmojicodeString, Type> map, std::vector<Type> constraints, size_t superCount, TypeContext tc) {
    printf("\"genericArguments\": [");
    
    auto gans = std::vector<EmojicodeString>(map.size());
    for (auto it : map) {
        gans[it.second.reference - superCount] = it.first;
    }
    
    auto reported = false;
    
    for (size_t i = 0; i < gans.size(); i++) {
        auto gan = gans[i];
        auto utf8 = gan.utf8CString();
        if (reported) {
            printf(",");
        }
        printf("{\"name\": ");
        printJSONStringToFile(utf8, stdout);
        printf(",");
        reportType("constraint", constraints[i], true, tc);
        printf("}%s", i + 1 == gans.size() ? "" : ",");
        delete [] utf8;
        reported = true;
    }
    
    printf("],");
}

void reportProcedureInformation(Procedure *p, ReturnManner returnm, bool last, TypeContext tc){
    ecCharToCharStack(p->name, nameString);
    
    printf("{");
    printf("\"name\": \"%s\",", nameString);
    
    if (returnm == Return) {
        reportType("returnType", p->returnType, false, tc);
    }
    else if (returnm == CanReturnNothingness) {
        printf("\"canReturnNothingness\": true,");
    }
    
    reportGenericArguments(p->genericArgumentVariables, p->genericArgumentConstraints, 0, tc);
    reportDocumentation(p->documentationToken);
    
    printf("\"arguments\": [");
    for (int i = 0; i < p->arguments.size(); i++) {
        printf("{");
        Variable variable = p->arguments[i];
        
        const char *varname = variable.name->value.utf8CString();
        
        reportType("type", variable.type, false, tc);
        printf("\"name\":");
        printJSONStringToFile(varname, stdout);
        printf("}%s", i + 1 == p->arguments.size() ? "" : ",");
        
        delete [] varname;
    }
    printf("]");
    
    printf("}%s", last ? "" : ",");
}

void report(const char *packageName){
//    bool printedClass = false;
//    printf("{");
//    printf("\"classes\": [");
//    for(auto eclass : classes){
//        if (strcmp(eclass->package->name(), packageName) != 0) {
//            continue;
//        }
//        
//        if (printedClass) {
//            putchar(',');
//        }
//        printedClass = true;
//        
//        printf("{");
//        
//        ecCharToCharStack(eclass->name, className);
//        printf("\"name\": \"%s\",", className);
//
//        reportGenericArguments(eclass->ownGenericArgumentVariables, eclass->genericArgumentContraints, eclass->superGenericArguments.size(), TypeContext(eclass));
//        reportDocumentation(eclass->documentationToken);
//        
//        if (eclass->superclass) {
//            ecCharToCharStack(eclass->superclass->name, superClassName);
//            printf("\"superclass\": {\"package\": \"%s\", \"name\": \"%s\"},", eclass->superclass->package->name(), superClassName);
//        }
//        
//        printf("\"methods\": [");
//        for(size_t i = 0; i < eclass->methodList.size(); i++){
//            Method *method = eclass->methodList[i];
//            reportProcedureInformation(method, Return, i + 1 == eclass->methodList.size(), TypeContext(eclass, method));
//        }
//        printf("],");
//        
//        printf("\"initializers\": [");
//        for(size_t i = 0; i < eclass->initializerList.size(); i++){
//            Initializer *initializer = eclass->initializerList[i];
//            reportProcedureInformation(initializer, initializer->canReturnNothingness ? CanReturnNothingness : NoReturn, i + 1 == eclass->initializerList.size(), TypeContext(eclass, initializer));
//        }
//        printf("],");
//        
//        printf("\"classMethods\": [");
//        for(size_t i = 0; i < eclass->classMethodList.size(); i++){
//            ClassMethod *classMethod = eclass->classMethodList[i];
//            reportProcedureInformation((Procedure *)classMethod, Return, eclass->classMethodList.size() == i + 1, TypeContext(eclass, classMethod));
//        }
//        printf("],");
//        
//        printf("\"conformsTo\": [");
//        for(size_t i = 0; i < eclass->protocols().size(); i++){
//            Protocol *protocol = eclass->protocols()[i];
//            reportType(nullptr, Type(protocol, false), i + 1 == eclass->protocols().size(), TypeContext(eclass));
//        }
//        printf("]}");
//    }
//    printf("],");
//    printf("\"enums\": [");
//    bool printedEnum = false;
//    for(auto it : enumsRegister){
//        Enum *eenum = it.second;
//        
//        if (strcmp(eenum->package->name(), packageName) != 0) {
//            continue;
//        }
//        if (printedEnum) {
//            putchar(',');
//        }
//        printf("{");
//        
//        printedEnum = true;
//        
//        ecCharToCharStack(eenum->name, enumName);
//        printf("\"name\": \"%s\",", enumName);
//        
//        reportDocumentation(eenum->documentationToken);
//        
//        bool printedValue = false;
//        printf("\"values\": [");
//        for(auto it : eenum->map){
//            ecCharToCharStack(it.first, value);
//            if (printedValue){
//                putchar(',');
//            }
//            printf("\"%s\"", value);
//            printedValue = true;
//        }
//        printf("]}");
//    }
//    printf("],");
//    printf("\"protocols\": [");
//    bool printedProtocol = false;
//    for(auto it : protocolsRegister){
//        Protocol *protocol = it.second;
//        
//        if (strcmp(protocol->package->name(), packageName) != 0) {
//            continue;
//        }
//        if (printedProtocol) {
//            putchar(',');
//        }
//        printedProtocol = true;
//        printf("{");
//        
//        ecCharToCharStack(protocol->name, protocolName);
//        printf("\"name\": \"%s\",", protocolName);
//        
//        reportDocumentation(protocol->documentationToken);
//        
//        printf("\"methods\": [");
//        for(size_t i = 0; i < protocol->methods().size(); i++){
//            Method *method = protocol->methods()[i];
//            reportProcedureInformation((Procedure *)method, Return, i + 1 == protocol->methods().size(), TypeContext(Type(protocol, false)));
//        }
//        printf("]}");
//    }
//    printf("]");
//    printf("}");
}