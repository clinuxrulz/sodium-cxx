/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#ifndef _SODIUM_DECL_H_
#define _SODIUM_DECL_H_

#include <sodium/light_ptr.h>
#include <sodium/transaction.h>
#include <functional>
#include <boost/optional.hpp>
#include <memory>
#include <list>
#if defined(SODIUM_NO_EXCEPTIONS)
#include <stdlib.h>
#else
#include <stdexcept>
#endif

// TO DO:
// the sample_lazy() mechanism is not correct yet. The lazy value needs to be
// fixed at the end of the transaction.

namespace sodium {

    template <typename A> class stream;
    template <typename A> class cell;
    template <typename A> class stream_loop;
    template <typename A, typename Selector> class router;
    template <typename A, typename B>
        cell<B> apply(const cell<std::function<B(const A&)>>& bf, const cell<A>& ba);
    template <typename A> stream<A>
        filter_optional(const stream<boost::optional<A>>& input);
    template <typename A> stream<A>
        split(const stream<std::list<A>>& e);

    namespace impl {

        class cell_;
        struct cell_impl;

        class stream_ {
        friend class cell_;
        template <typename A> friend class sodium::stream;
        template <typename A> friend class sodium::stream_loop;
        template <typename A> friend class sodium::cell;
        friend cell_ switch_c(transaction_impl* trans, const cell_& bba);
        friend SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0, const light_ptr& initValue, const stream_& input);
        friend SODIUM_SHARED_PTR<cell_impl> hold_lazy(transaction_impl* trans0, const std::function<light_ptr()>& initValue, const stream_& input);
        template <typename A, typename B>
        friend cell<B> sodium::apply(const cell<std::function<B(const A&)>>& bf, const cell<A>& ba);
        friend cell_ apply(transaction_impl* trans0, const cell_& bf, const cell_& ba);
        friend stream_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f, const stream_& ev);
        friend cell_ map_(transaction_impl* trans,
            const std::function<light_ptr(const light_ptr&)>& f,
            const cell_& beh);
        friend stream_ switch_s(transaction_impl* trans, const cell_& bea);
        template <typename A>
        friend stream<A> sodium::split(const stream<std::list<A>>& e);
        friend stream_ filter_optional_(transaction_impl* trans, const stream_& input,
            const std::function<boost::optional<light_ptr>(const light_ptr&)>& f);
        template <typename A, typename Selector> friend class sodium::router;

        protected:
            boost::intrusive_ptr<listen_impl_func<H_STREAM> > p_listen_impl;

        public:
            inline stream_();
            inline stream_(boost::intrusive_ptr<listen_impl_func<H_STREAM>> p_listen_impl_)
                : p_listen_impl(std::move(p_listen_impl_)) {}

#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            bool is_never() const { return !impl::alive(p_listen_impl); }
#endif

        protected:

            /*!
             * listen to streams.
             */
            inline std::function<void()>* listen_raw(
                        transaction_impl* trans0,
                        const SODIUM_SHARED_PTR<impl::node>& target,
                        std::function<void(const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&)>* handle,
                        bool suppressEarlierFirings) const;

            /*!
             * This is far more efficient than add_cleanup because it modifies the stream
             * in place.
             */
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup)
            {
                boost::intrusive_ptr<listen_impl_func<H_STRONG> > li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(p_listen_impl.get()));
                if (cleanup != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup);
                    else {
                        (*cleanup)();
                        delete cleanup;
                    }
                }
                return *this;
            }

            /*!
             * This is far more efficient than add_cleanup because it modifies the stream
             * in place.
             */
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup1, std::function<void()>* cleanup2)
            {
                boost::intrusive_ptr<listen_impl_func<H_STRONG> > li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(p_listen_impl.get()));
                if (cleanup1 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup1);
                    else {
                        (*cleanup1)();
                        delete cleanup1;
                    }
                }
                if (cleanup2 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup2);
                    else {
                        (*cleanup2)();
                        delete cleanup2;
                    }
                }
                return *this;
            }

            /*!
             * This is far more efficient than add_cleanup because it modifies the stream
             * in place.
             */
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup1, std::function<void()>* cleanup2, std::function<void()>* cleanup3)
            {
                boost::intrusive_ptr<listen_impl_func<H_STRONG> > li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(p_listen_impl.get()));
                if (cleanup1 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup1);
                    else {
                        (*cleanup1)();
                        delete cleanup1;
                    }
                }
                if (cleanup2 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup2);
                    else {
                        (*cleanup2)();
                        delete cleanup2;
                    }
                }
                if (cleanup3 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup3);
                    else {
                        (*cleanup3)();
                        delete cleanup3;
                    }
                }
                return *this;
            }

            /*!
             * Create a new stream that is like this stream but has an extra cleanup.
             */
            inline stream_ add_cleanup_(transaction_impl* trans, std::function<void()>* cleanup) const;
            inline cell_ hold_(transaction_impl* trans, const light_ptr& initA) const;
            inline cell_ hold_lazy_(transaction_impl* trans, const std::function<light_ptr()>& initA) const;
            inline stream_ once_(transaction_impl* trans) const;
            inline stream_ merge_(transaction_impl* trans, const stream_& other) const;
            inline stream_ coalesce_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine) const;
            inline stream_ last_firing_only_(transaction_impl* trans) const;
            inline stream_ snapshot_(transaction_impl* trans, const cell_& beh, const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine) const;
            inline stream_ filter_(transaction_impl* trans, const std::function<bool(const light_ptr&)>& pred) const;

            std::function<void()>* listen_impl(
                transaction_impl* trans,
                const SODIUM_SHARED_PTR<impl::node>& target,
                SODIUM_SHARED_PTR<holder> h,
                bool suppressEarlierFirings) const
            {
                boost::intrusive_ptr<listen_impl_func<H_STRONG> > li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(p_listen_impl.get()));
                if (alive(li))
                    return (*li->func)(trans, target, h, suppressEarlierFirings);
                else
                    return NULL;
            }
        };
        #define SODIUM_DETYPE_FUNCTION1(A,B,f) \
                   [f] (const light_ptr& a) -> light_ptr { \
                        return light_ptr::create<B>(f(*a.cast_ptr<A>(NULL))); \
                   }
        inline stream_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f, const stream_& ca);

        /*!
         * Function to push a value into an stream
         */
        inline void send(const SODIUM_SHARED_PTR<node>& n, transaction_impl* trans, const light_ptr& ptr);

        /*!
         * Creates an stream, that values can be pushed into using impl::send(). 
         */
        inline SODIUM_TUPLE<
                stream_,
                SODIUM_SHARED_PTR<node>
            > unsafe_new_stream();

        struct cell_impl {
            inline cell_impl();
            inline cell_impl(
                const stream_& updates,
                const SODIUM_SHARED_PTR<cell_impl>& parent);
            inline virtual ~cell_impl();

            virtual const light_ptr& sample() const = 0;
            virtual const light_ptr& newValue() const = 0;

            stream_ updates;  // Having this here allows references to cell to keep the
                             // underlying stream's cleanups alive, and provides access to the
                             // underlying stream, for certain primitives.

            std::function<void()>* kill;
            SODIUM_SHARED_PTR<cell_impl> parent;

            std::function<std::function<void()>(transaction_impl*, const SODIUM_SHARED_PTR<node>&,
                             const std::function<void(transaction_impl*, const light_ptr&)>&)> listen_value_raw() const;
        };

        inline SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0,
                            const light_ptr& initValue,
                            const stream_& input);
        inline SODIUM_SHARED_PTR<cell_impl> hold_lazy(transaction_impl* trans0,
                            const std::function<light_ptr()>& initValue,
                            const stream_& input);

        struct cell_impl_constant : cell_impl {
            cell_impl_constant(light_ptr k_) : k(std::move(k_)) {}
            light_ptr k;
            virtual const light_ptr& sample() const { return k; }
            virtual const light_ptr& newValue() const { return k; }
        };

        template <typename state_t>
        struct cell_impl_concrete : cell_impl {
            cell_impl_concrete(
                const stream_& updates_,
                state_t&& state_,
                const SODIUM_SHARED_PTR<cell_impl>& parent_)
            : cell_impl(updates_, parent_),
              state(std::move(state_))
            {
            }
            state_t state;

            virtual const light_ptr& sample() const { return state.sample(); }
            virtual const light_ptr& newValue() const { return state.newValue(); }
        };

        struct cell_impl_loop : cell_impl {
            cell_impl_loop(
                const stream_& updates_,
                const SODIUM_SHARED_PTR<SODIUM_SHARED_PTR<cell_impl> >& pLooped_,
                const SODIUM_SHARED_PTR<cell_impl>& parent_)
            : cell_impl(updates_, parent_),
              pLooped(pLooped_)
            {
            }
            SODIUM_SHARED_PTR<SODIUM_SHARED_PTR<cell_impl> > pLooped;

            void assertLooped() const {
                if (!*pLooped)
                    SODIUM_THROW("cell_loop sampled before it was looped");
            }

            virtual const light_ptr& sample() const { assertLooped(); return (*pLooped)->sample(); }
            virtual const light_ptr& newValue() const { assertLooped(); return (*pLooped)->newValue(); }
        };

        struct cell_state {
            cell_state(const light_ptr& initA) : current(initA) {}
            light_ptr current;
            boost::optional<light_ptr> update;
            const light_ptr& sample() const { return current; }
            const light_ptr& newValue() const { return update ? update.get() : current; }
            void finalize() {
                current = update.get();
                update = boost::optional<light_ptr>();
            }
        };

        struct cell_state_lazy {
        private:
            // Don't allow copying because we have no valid implementation for that
            // given our use of a pointer in pInitA.
            cell_state_lazy(const cell_state_lazy&) {}
            cell_state_lazy& operator = (const cell_state_lazy&) {
                return *this;
            }
        public:
            cell_state_lazy(const std::function<light_ptr()>& initA)
            : pInitA(new std::function<light_ptr()>(initA)) {}
            cell_state_lazy(cell_state_lazy&& other)
            : pInitA(other.pInitA),
              current(other.current),
              update(other.update)
            {
                other.pInitA = NULL;
            }
            ~cell_state_lazy()
            {
                delete pInitA;
            }
            std::function<light_ptr()>* pInitA;
            boost::optional<light_ptr> current;
            boost::optional<light_ptr> update;
            const light_ptr& sample() const {
                if (!current) {
                    const_cast<cell_state_lazy*>(this)->current = boost::optional<light_ptr>((*pInitA)());
                    delete pInitA;
                    const_cast<cell_state_lazy*>(this)->pInitA = NULL;
                }
                return current.get();
            }
            const light_ptr& newValue() const { return update ? update.get() : sample(); }
            void finalize() {
                current = update;
                update = boost::optional<light_ptr>();
            }
        };

        class cell_ {
            friend impl::stream_ underlying_stream(const cell_& beh);
            public:
                inline cell_();
                inline cell_(cell_impl* impl);
                inline cell_(SODIUM_SHARED_PTR<cell_impl> impl);
                inline cell_(light_ptr a);
                SODIUM_SHARED_PTR<impl::cell_impl> impl;

#if defined(SODIUM_CONSTANT_OPTIMIZATION)
                /*!
                 * For optimization, if this cell is a constant, then return its value.
                 */
                inline boost::optional<light_ptr> get_constant_value() const;
#endif

                inline stream_ value_(transaction_impl* trans) const;
                const stream_& updates_() const { return impl->updates; }
        };

        inline cell_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f,
            const cell_& beh);

        struct stream_sink_impl {
            inline stream_sink_impl();
            inline stream_ construct();
            inline void send(transaction_impl* trans, const light_ptr& ptr) const;
            SODIUM_SHARED_PTR<impl::node> target;
        };

        template <typename A, typename L>
        stream<A> merge(const L& sas, size_t start, size_t end, const std::function<A(const A&, const A&)>& f) {
            size_t len = end - start;
            if (len == 0) return stream<A>(); else
            if (len == 1) return sas[start]; else
            if (len == 2) return sas[start].merge(sas[start+1], f); else {
                int mid = (start + end) / 2;
                return merge<A,L>(sas, start, mid, f).merge(merge<A,L>(sas, mid, end, f), f);
            }
        }

        inline stream_ filter_optional_(transaction_impl* trans, const stream_& input,
            const std::function<boost::optional<light_ptr>(const light_ptr&)>& f);

        /*!
         * Returns an stream describing the changes in a cell.
         */
        inline impl::stream_ underlying_stream(const impl::cell_& beh) {return beh.impl->updates;}

        inline cell_ apply(transaction_impl* trans, const cell_& bf, const cell_& ba);

        inline stream_ switch_s(transaction_impl* trans, const cell_& bea);

        inline cell_ switch_c(transaction_impl* trans, const cell_& bba);

    } // namespace impl

}  // namespace sodium
#endif // _SODIUM_DECL_H_

