#ifndef SPHINXSYS_EXECUTION_PROXY_HPP
#define SPHINXSYS_EXECUTION_PROXY_HPP

#include "execution_policy.h"
#include "execution_context.hpp"

namespace SPH {
    namespace execution {
        template<typename BaseT, typename KernelT>
        class ExecutionProxy {
        public:
            using Base = BaseT;
            using Kernel = KernelT;

            ExecutionProxy(BaseT* base, KernelT* proxy) : base(base), kernel(proxy) {}

            template<class ExecutionPolicy = ParallelPolicy>
            BaseT* get(const ExecutionPolicy& = par,
                       std::enable_if_t<std::negation_v<std::is_same<ExecutionPolicy, ParallelSYCLDevicePolicy>>>* = nullptr) const {
                return base;
            }

            KernelT* get(const ParallelSYCLDevicePolicy&) const {
                return kernel;
            }

            template<class ExecutionPolicy>
            void get_memory_access(const Context<ExecutionPolicy>& context, const ExecutionPolicy&) {
                if constexpr (std::is_same_v<ExecutionPolicy, ParallelSYCLDevicePolicy>)
                    this->get_memory_access_device(context.cgh);
            }

        protected:
            virtual void get_memory_access_device(sycl::handler& cgh) = 0;

            BaseT* base;
            KernelT* kernel;
        };


        template<typename T>
        class NoProxy : public ExecutionProxy<T, T> {
        public:
            explicit NoProxy(T *base) : ExecutionProxy<T, T>(base, base) {}

        protected:
            void get_memory_access_device(sycl::handler& cgh) override {
                static_assert("No device memory access available for this class.");
            }
        };
    }
}

#endif //SPHINXSYS_EXECUTION_PROXY_HPP
