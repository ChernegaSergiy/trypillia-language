import re

with open("src/vm/VM.cpp", "r") as f:
    content = f.read()

# Replace `method->isAbstract`
content = re.sub(
    r'method->isAbstract',
    r'isMethodAbstract(method)',
    content
)

# Replace `method->accessModifier`
content = re.sub(
    r'method->accessModifier',
    r'getMethodAccessModifier(method)',
    content
)

# Replace OP_ABSTRACT_METHOD setting isAbstract
content = re.sub(
    r'auto method = std::get<std::shared_ptr<ObjFunction>>\(methodVal\);\n\s*method->isAbstract = true;',
    r'if (std::holds_alternative<std::shared_ptr<ObjFunction>>(methodVal)) std::get<std::shared_ptr<ObjFunction>>(methodVal)->isAbstract = true; else std::get<std::shared_ptr<ObjNative>>(methodVal)->isAbstract = true;',
    content
)

# Fix bound method OP_CALL
bound_method_call = """
                    auto bound = std::get<std::shared_ptr<ObjBoundMethod>>(callee);
                    auto function = bound->method;
                    if (isMethodAbstract(function)) {
                        std::cerr << "Cannot call abstract method '" << getMethodName(function) << "'." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    if (getMethodArity(function) != -1 && argCount != getMethodArity(function)) {
                        std::cerr << "Expected " << getMethodArity(function) << " arguments but got " << (int)argCount << "." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    if (frames.size() == 256) {
                        std::cerr << "Stack overflow." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    stack[stack.size() - argCount - 1] = bound->receiver;
                    
                    if (std::holds_alternative<std::shared_ptr<ObjFunction>>(function)) {
                        auto func = std::get<std::shared_ptr<ObjFunction>>(function);
                        CallFrame newFrame;
                        newFrame.function = func;
                        newFrame.ip = func->chunk->code.data();
                        newFrame.stackStart = stack.size() - argCount - 1;
                        frames.push_back(newFrame);
                        frame = &frames.back();
                    } else {
                        auto native = std::get<std::shared_ptr<ObjNative>>(function);
                        VMValue result = native->function(argCount, stack.data() + stack.size() - argCount);
                        stack.resize(stack.size() - argCount - 1);
                        push(result);
                    }
"""

content = re.sub(
    r'                    auto bound = std::get<std::shared_ptr<ObjBoundMethod>>\(callee\);\n.*?frames\.push_back\(newFrame\);\n\s*frame = &frames\.back\(\);',
    bound_method_call.strip("\n"),
    content,
    flags=re.DOTALL
)

# Fix initMethod OP_CLASS CallFrame
init_method_call = """
                        if (getMethodArity(initMethod) != -1 && argCount != getMethodArity(initMethod)) {
                            std::cerr << "Expected " << getMethodArity(initMethod) << " arguments but got " << argCount << "." << std::endl;
                            return InterpretResult::INTERPRET_RUNTIME_ERROR;
                        }
                        
                        if (std::holds_alternative<std::shared_ptr<ObjFunction>>(initMethod)) {
                            auto func = std::get<std::shared_ptr<ObjFunction>>(initMethod);
                            CallFrame newFrame;
                            newFrame.function = func;
                            newFrame.ip = func->chunk->code.data();
                            newFrame.stackStart = stack.size() - argCount - 1;
                            frames.push_back(newFrame);
                            frame = &frames.back();
                        } else {
                            auto native = std::get<std::shared_ptr<ObjNative>>(initMethod);
                            native->function(argCount, stack.data() + stack.size() - argCount);
                            stack.resize(stack.size() - argCount); // leave the instance on stack
                        }
"""

content = re.sub(
    r'                        if \(argCount != initMethod->arity\).*?frames\.push_back\(newFrame\);\n\s*frame = &frames\.back\(\);',
    init_method_call.strip("\n"),
    content,
    flags=re.DOTALL
)

with open("src/vm/VM.cpp", "w") as f:
    f.write(content)
