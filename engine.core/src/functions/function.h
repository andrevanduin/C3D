
#pragma once
#include "defines.h"
#include "logger/logger.h"

/* TODO: Rework into nice implementation based on JUCE's FixedSizeFunction:
 * https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/containers/juce_FixedSizeFunction.h
 */
namespace C3D
{
    template <typename... Args>
    class Function;

    template <typename Result, typename... Args>
    class Function<Result(Args...)>
    {
    public:
        Function() = default;

        Function(const Function&)            = delete;
        Function& operator=(const Function&) = delete;

        Function(Function&&)            = delete;
        Function& operator=(Function&&) = delete;

        virtual ~Function()                      = default;
        virtual Result operator()(Args...) const = 0;
    };

    template <typename, u64>
    class StackFunction;

    template <u64 stackSize, typename Result, typename... Args>
    class StackFunction<Result(Args...), stackSize> final : public Function<Result(Args...)>
    {
    public:
        StackFunction() = default;

        template <typename Functor>
        StackFunction(Functor func)
        {
            static_assert((sizeof(FunctorHolder<Functor, Result, Args...>) <= sizeof(m_stack)),
                          "Functor is too large to fit on defined stack (increase stackSize)");

            m_functorHolderPtr = reinterpret_cast<IFunctorHolder<Result, Args...>*>(std::addressof(m_stack));
            new (m_functorHolderPtr) FunctorHolder<Functor, Result, Args...>(func);
        }

        StackFunction(const StackFunction& other)
        {
            if (other.m_functorHolderPtr != nullptr)
            {
                m_functorHolderPtr = reinterpret_cast<IFunctorHolder<Result, Args...>*>(std::addressof(m_stack));
                other.m_functorHolderPtr->CopyInto(m_functorHolderPtr);
            }
        }

        /*
        StackFunction(StackFunction&& other) noexcept
        {
            if (other.m_functorHolderPtr != nullptr)
            {
                m_functorHolderPtr = reinterpret_cast<IFunctorHolder<Result, Args...>*>(std::addressof(m_stack));
                other.m_functorHolderPtr->CopyInto(m_functorHolderPtr);
                other.m_functorHolderPtr = nullptr;
            }
        }

        StackFunction& operator=(StackFunction&& other) noexcept
        {
            if (m_functorHolderPtr != nullptr)
            {
                m_functorHolderPtr->~IFunctorHolder<Result, Args...>();
                m_functorHolderPtr = nullptr;
            }

            if (other.m_functorHolderPtr != nullptr)
            {
                m_functorHolderPtr = reinterpret_cast<IFunctorHolder<Result, Args...>*>(std::addressof(m_stack));
                other.m_functorHolderPtr->CopyInto(m_functorHolderPtr);
                other.m_functorHolderPtr = nullptr;
            }

            return *this;
        }*/

        StackFunction& operator=(const StackFunction& other)
        {
            if (this != &other)
            {
                if (m_functorHolderPtr != nullptr)
                {
                    m_functorHolderPtr->~IFunctorHolder<Result, Args...>();
                    m_functorHolderPtr = nullptr;
                }

                if (other.m_functorHolderPtr != nullptr)
                {
                    m_functorHolderPtr = reinterpret_cast<IFunctorHolder<Result, Args...>*>(std::addressof(m_stack));
                    other.m_functorHolderPtr->CopyInto(m_functorHolderPtr);
                }
            }

            return *this;
        }

        ~StackFunction() override
        {
            if (m_functorHolderPtr != nullptr)
            {
                m_functorHolderPtr->~IFunctorHolder<Result, Args...>();
            }
        }

        Result operator()(Args... args) const override { return (*m_functorHolderPtr)(std::forward<Args>(args)...); }

        operator bool() const noexcept { return m_functorHolderPtr != nullptr; }

        bool operator==(const StackFunction& other) const { return std::memcmp(m_stack, other.m_stack, stackSize) == 0; }

    private:
        template <typename ReturnType, typename... Arguments>
        struct IFunctorHolder
        {
            virtual ~IFunctorHolder() {}
            virtual ReturnType operator()(Arguments...) = 0;
            virtual void CopyInto(void*) const          = 0;
        };

        template <typename Functor, typename ReturnType, typename... Arguments>
        struct FunctorHolder final : IFunctorHolder<Result, Arguments...>
        {
            explicit FunctorHolder(Functor func) : functor(func) {}

            ReturnType operator()(Arguments... args) override { return functor(std::forward<Arguments>(args)...); }

            void CopyInto(void* destination) const override { new (destination) FunctorHolder(functor); }

            Functor functor;
        };

        IFunctorHolder<Result, Args...>* m_functorHolderPtr = nullptr;
        byte m_stack[stackSize]                             = { 0 };
    };
}  // namespace C3D