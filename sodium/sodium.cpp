/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#include <sodium/sodium.h>

using namespace std;
using namespace boost;


namespace sodium {

    namespace impl {

        /*!
         * Creates an stream, that values can be pushed into using impl::send(). 
         */
        SODIUM_TUPLE<stream_, SODIUM_SHARED_PTR<node> > unsafe_new_stream()
        {
            SODIUM_SHARED_PTR<node> n1(new node);
            SODIUM_WEAK_PTR<node> n_weak(n1);
            boost::intrusive_ptr<listen_impl_func<H_STRONG> > impl(
                new listen_impl_func<H_STRONG>(new listen_impl_func<H_STRONG>::closure([n_weak] (transaction_impl* trans1,
                        const SODIUM_SHARED_PTR<node>& target,
                        const SODIUM_SHARED_PTR<holder>& h,
                        bool suppressEarlierFirings) -> std::function<void()>* {  // Register listener
                    SODIUM_SHARED_PTR<node> n2 = n_weak.lock();
                    if (n2) {
#if !defined(SODIUM_SINGLE_THREADED)
                        transaction_impl::part->mx.lock();
#endif
                        if (n2->link(h.get(), target))
                            trans1->to_regen = true;
#if !defined(SODIUM_SINGLE_THREADED)
                        transaction_impl::part->mx.unlock();
#endif
                        if (!suppressEarlierFirings && n2->firings.begin() != n2->firings.end()) {
                            SODIUM_FORWARD_LIST<light_ptr> firings = n2->firings;
                            trans1->prioritized(target, [target, h, firings] (transaction_impl* trans2) {
                                for (SODIUM_FORWARD_LIST<light_ptr>::const_iterator it = firings.begin(); it != firings.end(); it++)
                                    h->handle(target, trans2, *it);
                            });
                        }
                        SODIUM_SHARED_PTR<holder>* h_keepalive = new SODIUM_SHARED_PTR<holder>(h);
                        return new std::function<void()>([n_weak, h_keepalive] () {  // Unregister listener
                            impl::transaction_ trans2;
                            trans2.impl()->last([n_weak, h_keepalive] () {
                                std::shared_ptr<node> n3 = n_weak.lock();
                                if (n3)
                                    n3->unlink((*h_keepalive).get());
                                delete h_keepalive;
                            });
                        });
                    }
                    else
                        return NULL;
                }))
            );
            n1->listen_impl = boost::intrusive_ptr<listen_impl_func<H_NODE> >(
                reinterpret_cast<listen_impl_func<H_NODE>*>(impl.get()));
            boost::intrusive_ptr<listen_impl_func<H_STREAM> > li_stream(
                reinterpret_cast<listen_impl_func<H_STREAM>*>(impl.get()));
            return SODIUM_MAKE_TUPLE(stream_(li_stream), n1);
        }

        cell_ map_(transaction_impl* trans,
            const std::function<light_ptr(const light_ptr&)>& f,
            const cell_& beh) {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            boost::optional<light_ptr> ca = beh.get_constant_value();
            if (ca)
                return cell_(f(ca.get()));
            else {
#endif
                auto impl = beh.impl;
                return map_(trans, f, beh.updates_()).hold_lazy_(trans, [f, impl] () -> light_ptr {
                    return f(impl->sample());
                });
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            }
#endif
        }

        stream_ switch_s(transaction_impl* trans0, const cell_& bea)
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = unsafe_new_stream();
            const SODIUM_SHARED_PTR<impl::node>& target1 = SODIUM_TUPLE_GET<1>(p);
            std::shared_ptr<function<void()>*> pKillInner(new function<void()>*(NULL));
            trans0->prioritized(target1, [pKillInner, bea, target1] (transaction_impl* trans) {
                if (*pKillInner == NULL)
                    *pKillInner = bea.impl->sample().cast_ptr<stream_>(NULL)->listen_raw(trans, target1, NULL, false);
            });

            auto killOuter = bea.updates_().listen_raw(trans0, target1,
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [pKillInner] (const std::shared_ptr<impl::node>& target2, impl::transaction_impl* trans1, const light_ptr& pea) {
                        const stream_& ea = *pea.cast_ptr<stream_>(NULL);
                        trans1->last([pKillInner, ea, target2, trans1] () {
                            KILL_ONCE(pKillInner);
                            *pKillInner = ea.listen_raw(trans1, target2, NULL, true);
                        });
                    }),
                false
            );
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
                new std::function<void()>([pKillInner] {
                    KILL_ONCE(pKillInner);
                })
                , killOuter);
        }

        cell_ switch_c(transaction_impl* trans0, const cell_& bba)
        {
            auto za = [bba] () -> light_ptr { return bba.impl->sample().cast_ptr<cell_>(NULL)->impl->sample(); };
            SODIUM_SHARED_PTR<function<void()>*> pKillInner(new function<void()>*(NULL));
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = unsafe_new_stream();
            auto out_target = SODIUM_TUPLE_GET<1>(p);
            auto killOuter =
                bba.value_(trans0).listen_raw(trans0, out_target,
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [pKillInner] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& pa) {
                        // Note: If any switch takes place during a transaction, then the
                        // value().listen will always cause a sample to be fetched from the
                        // one we just switched to. The caller will be fetching our output
                        // using value().listen, and value() throws away all firings except
                        // for the last one. Therefore, anything from the old input cell
                        // that might have happened during this transaction will be suppressed.
                        KILL_ONCE(pKillInner);
                        const cell_& ba = *pa.cast_ptr<cell_>(NULL);
                        *pKillInner = ba.value_(trans).listen_raw(trans, target, NULL, false);
                    })
                , false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
                new std::function<void()>([pKillInner] {
                    KILL_ONCE(pKillInner);
                })
                , killOuter).hold_lazy_(trans0, za);
        }

        stream_ filter_optional_(transaction_impl* trans1, const stream_& input,
            const std::function<boost::optional<light_ptr>(const light_ptr&)>& f)
        {
            auto p = impl::unsafe_new_stream();
            auto kill = input.listen_raw(trans1, std::get<1>(p),
                new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                    [f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& poa) {
                        boost::optional<light_ptr> oa = f(poa);
                        if (oa) impl::send(target, trans2, oa.get());
                    })
                , false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

    };  // end namespace impl
};  // end namespace sodium
