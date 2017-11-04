/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */

#ifndef _SODIUM_IMPL_H_
#define _SODIUM_IMPL_H_

namespace sodium {

    namespace impl {

        template <typename A>
        stream_<A>::stream_()
        {
        }

        /*!
         * listen to streams.
         */
        template <typename A>
        std::function<void()>* stream_<A>::listen_raw(
                    transaction_impl* trans,
                    const SODIUM_SHARED_PTR<impl::node>& target,
                    std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>* handler,
                    bool suppressEarlierFirings) const
        {
            SODIUM_SHARED_PTR<holder> h(new holder(handler));
            return listen_impl(trans, target, h, suppressEarlierFirings);
        }

        template <typename A>
        cell_<A> stream_<A>::hold_(transaction_impl* trans, const light_ptr& initA) const
        {
            return cell_<A>(
                SODIUM_SHARED_PTR<impl::cell_impl<A>>(impl::hold(trans, initA, *this))
            );
        }

        template <typename A>
        cell_<A> stream_<A>::hold_lazy_(transaction_impl* trans, const std::function<light_ptr()>& initA) const
        {
            return cell_<A>(
                SODIUM_SHARED_PTR<impl::cell_impl<A>>(impl::hold_lazy(trans, initA, *this))
            );
        }

        #define KILL_ONCE(ppKill) \
            do { \
                std::function<void()>* pKill = *ppKill; \
                if (pKill != NULL) { \
                    *ppKill = NULL; \
                    (*pKill)(); \
                    delete pKill; \
                } \
            } while (0)

        template <typename A>
        stream_<A> stream_<A>::once_(transaction_impl* trans1) const
        {
            SODIUM_SHARED_PTR<std::function<void()>*> ppKill(new std::function<void()>*(NULL));

            SODIUM_TUPLE<impl::stream_<A>,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream<A>();
            *ppKill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [ppKill] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                        if (*ppKill) {
                            send(target, trans2, ptr);
                            KILL_ONCE(ppKill);
                        }
                    }),
                false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
                new std::function<void()>([ppKill] () {
                    KILL_ONCE(ppKill);
                })
            );
        }

        template <typename A>
        stream_<A> stream_<A>::merge_(transaction_impl* trans1, const stream_<A>& other) const {
            SODIUM_TUPLE<impl::stream_<A>,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream<A>();
            SODIUM_SHARED_PTR<impl::node> left(new impl::node);
            const SODIUM_SHARED_PTR<impl::node>& right = SODIUM_TUPLE_GET<1>(p);
            char* h = new char;
            if (left->link(h, right))
                trans1->to_regen = true;
            // defer right side to make sure merge is left-biased
            auto kill1 = this->listen_raw(trans1, left,
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [right] (const std::shared_ptr<impl::node>&, impl::transaction_impl* trans2, const light_ptr& a) {
                        send(right, trans2, a);
                    }), false);
            auto kill2 = other.listen_raw(trans1, right, NULL, false);
            auto kill3 = new std::function<void()>([left, h] () {
                left->unlink(h);
                delete h;
            });
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill1, kill2, kill3);
        }

        struct coalesce_state {
            coalesce_state() {}
            ~coalesce_state() {}
            boost::optional<light_ptr> oValue;
        };

        template <typename A>
        stream_<A> stream_<A>::coalesce_(transaction_impl* trans1,
                const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine
            ) const
        {
            SODIUM_SHARED_PTR<coalesce_state> pState(new coalesce_state);
            SODIUM_TUPLE<impl::stream_<A>,SODIUM_SHARED_PTR<impl::node>> p = impl::unsafe_new_stream<A>();
            auto kill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [pState, combine] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                        if (!pState->oValue) {
                            pState->oValue = boost::optional<light_ptr>(ptr);
                            trans2->prioritized(target, [target, pState] (transaction_impl* trans3) {
                                if (pState->oValue) {
                                    send(target, trans3, pState->oValue.get());
                                    pState->oValue = boost::optional<light_ptr>();
                                }
                            });
                        }
                        else
                            pState->oValue = boost::make_optional(combine(pState->oValue.get(), ptr));
                    }), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        template <typename A>
        stream_<A> stream_<A>::last_firing_only_(transaction_impl* trans) const
        {
            return coalesce_(trans, [] (const light_ptr& fst, const light_ptr& snd) {
                return snd;
            });
        }

        /*!
         * Sample the cell's value as at the transaction before the
         * current one, i.e. no changes from the current transaction are
         * taken.
         */
        template <typename A> template <typename B, typename C>
        stream_<C> stream_<A>::snapshot_(transaction_impl* trans1, const cell_<B>& beh,
                const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine
            ) const
        {
            SODIUM_TUPLE<impl::stream_<C>,SODIUM_SHARED_PTR<impl::node>> p = impl::unsafe_new_stream<C>();
            auto kill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                        [beh, combine] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& a) {
                        send(target, trans2, combine(a, beh.impl->sample()));
                    }), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        /*!
         * Filter this stream based on the specified predicate, passing through values
         * where the predicate returns true.
         */
        template <typename A>
        stream_<A> stream_<A>::filter_(transaction_impl* trans1,
                const std::function<bool(const light_ptr&)>& pred
            ) const
        {
            SODIUM_TUPLE<impl::stream_<A>,SODIUM_SHARED_PTR<impl::node>> p = impl::unsafe_new_stream<A>();
            auto kill = listen_raw(trans1, std::get<1>(p),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                        [pred] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            if (pred(ptr)) send(target, trans2, ptr);
                        }), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        template <typename A>
        cell_impl<A>::cell_impl()
            : updates(stream_<A>()),
              kill(NULL)
        {
        }

        template <typename A>
        cell_impl<A>::cell_impl(
            const stream_<A>& updates_,
            const SODIUM_SHARED_PTR<cell_impl<A>>& parent_)
            : updates(updates_), kill(NULL), parent(parent_)
        {
        }

        template <typename A>
        cell_impl<A>::~cell_impl()
        {
            if (kill) {
                (*kill)();
                delete kill;
            }
        }
        
        /*!
         * Function to push a value into an stream
         */
        void send(const SODIUM_SHARED_PTR<node>& n, transaction_impl* trans1, const light_ptr& a)
        {
            if (n->firings.begin() == n->firings.end())
                trans1->last([n] () {
                    n->firings.clear();
                });
            n->firings.push_front(a);
            SODIUM_FORWARD_LIST<node::target>::iterator it = n->targets.begin();
            while (it != n->targets.end()) {
                node::target* f = &*it;
                trans1->prioritized(f->n, [f, a] (transaction_impl* trans2) {
                    trans2->inCallback++;
                    try {
                        ((holder*)f->h)->handle(f->n, trans2, a);
                        trans2->inCallback--;
                    }
                    catch (...) {
                        trans2->inCallback--;
                        throw;
                    }
                });
                it++;
            }
        }

        /*!
         * Creates an stream, that values can be pushed into using impl::send(). 
         */
        template <typename A>
        SODIUM_TUPLE<stream_<A>, SODIUM_SHARED_PTR<node>> unsafe_new_stream()
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
            return SODIUM_MAKE_TUPLE(stream_<A>(li_stream), n1);
        }

        template <typename A>
        stream_sink_impl<A>::stream_sink_impl()
        {
        }

        template <typename A>
        stream_<A> stream_sink_impl<A>::construct()
        {
            SODIUM_TUPLE<impl::stream_<A>,SODIUM_SHARED_PTR<impl::node>> p = impl::unsafe_new_stream<A>();
            this->target = SODIUM_TUPLE_GET<1>(p);
            return SODIUM_TUPLE_GET<0>(p);
        }

        template <typename A>
        void stream_sink_impl<A>::send(transaction_impl* trans, const light_ptr& value) const
        {
            sodium::impl::send(target, trans, value);
        }


        template <typename A>
        SODIUM_SHARED_PTR<cell_impl<A>> hold(transaction_impl* trans0, const light_ptr& initValue, const stream_<A>& input)
        {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            if (input.is_never())
                return SODIUM_SHARED_PTR<cell_impl<A>>(new cell_impl_constant<A>(initValue));
            else {
#endif
                SODIUM_SHARED_PTR<cell_impl_concrete<A,cell_state<A>>> impl(
                    new cell_impl_concrete<A,cell_state<A>>(input, cell_state<A>(initValue), std::shared_ptr<cell_impl<A>>())
                );
                SODIUM_WEAK_PTR<cell_impl_concrete<A,cell_state<A>>> impl_weak(impl);
                impl->kill =
                    input.listen_raw(trans0, SODIUM_SHARED_PTR<node>(new node(SODIUM_IMPL_RANK_T_MAX)),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                        [impl_weak] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& ptr) {
                            SODIUM_SHARED_PTR<cell_impl_concrete<A,cell_state<A>>> impl_ = impl_weak.lock();
                            if (impl_) {
                                bool first = !impl_->state.update;
                                impl_->state.update = boost::optional<light_ptr>(ptr);
                                if (first)
                                    trans->last([impl_] () { impl_->state.finalize(); });
                                send(target, trans, ptr);
                            }
                        })
                    , false);
                return impl;
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            }
#endif
        }

        template <typename A>
        SODIUM_SHARED_PTR<cell_impl<A>> hold_lazy(transaction_impl* trans0, const std::function<light_ptr()>& initValue, const stream_<A>& input)
        {
            SODIUM_SHARED_PTR<cell_impl_concrete<A,cell_state_lazy<A>>> impl(
                new cell_impl_concrete<A,cell_state_lazy<A>>(input, cell_state_lazy<A>(initValue), std::shared_ptr<cell_impl<A>>())
            );
            SODIUM_WEAK_PTR<cell_impl_concrete<A,cell_state_lazy<A>> > w_impl(impl);
            impl->kill =
                input.listen_raw(trans0, SODIUM_SHARED_PTR<node>(new node(SODIUM_IMPL_RANK_T_MAX)),
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [w_impl] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& ptr) {
                        SODIUM_SHARED_PTR<cell_impl_concrete<A,cell_state_lazy<A>> > impl_ = w_impl.lock();
                        if (impl_) {
                            bool first = !impl_->state.update;
                            impl_->state.update = boost::optional<light_ptr>(ptr);
                            if (first)
                                trans->last([impl_] () { impl_->state.finalize(); });
                            send(target, trans, ptr);
                        }
                    })
                , false);
            return std::static_pointer_cast<cell_impl<A>, cell_impl_concrete<A,cell_state_lazy<A>>>(impl);
        }

        template <typename A>
        cell_<A>::cell_()
        {
        }

        template <typename A>
        cell_<A>::cell_(cell_impl<A>* impl_)
            : impl(impl_)
        {
        }

        template <typename A>
        cell_<A>::cell_(SODIUM_SHARED_PTR<cell_impl<A>> impl_)
            : impl(std::move(impl_))
        {
        }

        template <typename A>
        cell_<A>::cell_(light_ptr a)
            : impl(new cell_impl_constant<A>(std::move(a)))
        {
        }

        template <typename A>
        stream_<A> cell_<A>::value_(transaction_impl* trans) const
        {
            SODIUM_TUPLE<stream_<A>,SODIUM_SHARED_PTR<node>> p = unsafe_new_stream<A>();
            const stream_<A>& eSpark = std::get<0>(p);
            const SODIUM_SHARED_PTR<node>& node = std::get<1>(p);
            send(node, trans, light_ptr::create<unit>(unit()));
            stream_<A> eInitial = eSpark.template snapshot_<A,A>(trans, *this,
                [] (const light_ptr& a, const light_ptr& b) -> light_ptr {
                    return b;
                }
            );
            return eInitial.merge_(trans, impl->updates).last_firing_only_(trans);
        }

#if defined(SODIUM_CONSTANT_OPTIMIZATION)
        /*!
         * For optimization, if this cell is a constant, then return its value.
         */
        template <typename A>
        boost::optional<light_ptr> cell_<A>::get_constant_value() const
        {
            return impl->updates.is_never() ? boost::optional<light_ptr>(impl->sample())
                                            : boost::optional<light_ptr>();
        }
#endif

        struct applicative_state {
            applicative_state() : fired(false) {}
            bool fired;
            boost::optional<light_ptr> f;
            boost::optional<light_ptr> a;
        };

        template <typename A, typename B>
        cell_<B> apply(transaction_impl* trans0, const cell_<std::function<B(const A&)>>& bf, const cell_<A>& ba)
        {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            boost::optional<light_ptr> ocf = bf.get_constant_value();
            if (ocf) { // function is constant
                auto f = *ocf.get().cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                return impl::map_<A,B>(trans0, f, ba);  // map optimizes to a constant where ba is constant
            }
            else {
                boost::optional<light_ptr> oca = ba.get_constant_value();
                if (oca) {  // 'a' value is constant but function is not
                    const light_ptr& a = oca.get();
                    return impl::map_<std::function<B(const A&)>,B>(trans0, [a] (const light_ptr& pf) -> light_ptr {
                        const std::function<light_ptr(const light_ptr&)>& f =
                            *pf.cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                        return f(a);
                    }, bf);
                }
                else {
#endif
                    // Non-constant case
                    SODIUM_SHARED_PTR<applicative_state> state(new applicative_state);

                    SODIUM_SHARED_PTR<impl::node> in_target(new impl::node);
                    SODIUM_TUPLE<impl::stream_<B>,SODIUM_SHARED_PTR<impl::node>> p = impl::unsafe_new_stream<B>();
                    const SODIUM_SHARED_PTR<impl::node>& out_target = SODIUM_TUPLE_GET<1>(p);
                    char* h = new char;
                    if (in_target->link(h, out_target))
                        trans0->to_regen = true;
                    auto output = [state, out_target] (transaction_impl* trans) {
                        auto f = *state->f.get().cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                        send(out_target, trans, f(state->a.get()));
                        state->fired = false;
                    };
                    auto kill1 = bf.value_(trans0).listen_raw(trans0, in_target,
                            new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                                [state, out_target, output] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& f) {
                                    state->f = f;
                                    if (state->a) {
                                        if (state->fired) return;
                                        state->fired = true;
                                        trans->prioritized(out_target, output);
                                    }
                                }
                            ), false);
                    auto kill2 = ba.value_(trans0).listen_raw(trans0, in_target,
                            new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                                [state, out_target, output] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& a) {
                                    state->a = a;
                                    if (state->f) {
                                        if (state->fired) return;
                                        state->fired = true;
                                        trans->prioritized(out_target, output);
                                    }
                                }
                            ), false);
                    auto kill3 = new std::function<void()>([in_target, h] () {
                        in_target->unlink(h);
                        delete h;
                    });
                    return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill1, kill2, kill3).hold_lazy_(
                        trans0, [bf, ba] () -> light_ptr {
                            auto f = *bf.impl->sample().template cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                            return f(ba.impl->sample());
                        }
                    );
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
                }
            }
#endif
        }

        template <typename A>
        stream_<A> stream_<A>::add_cleanup_(transaction_impl* trans, std::function<void()>* cleanup) const
        {
            SODIUM_TUPLE<impl::stream_<A>,SODIUM_SHARED_PTR<impl::node>> p = impl::unsafe_new_stream<A>();
            auto kill = listen_raw(trans, std::get<1>(p),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(send),
                    false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill, cleanup);
        }

        /*!
         * Map a function over this stream to modify the output value.
         */
        template <typename A, typename B>
        stream_<B> map_(transaction_impl* trans1,
            const std::function<light_ptr(const light_ptr&)>& f,
            const stream_<A>& ev)
        {
            SODIUM_TUPLE<impl::stream_<B>,SODIUM_SHARED_PTR<impl::node>> p = impl::unsafe_new_stream<B>();
            auto kill = ev.listen_raw(trans1, std::get<1>(p),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                        [f] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            send(target, trans2, f(ptr));
                        }), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        template <typename A, typename B>
        cell_<B> map_(transaction_impl* trans,
            const std::function<light_ptr(const light_ptr&)>& f,
            const cell_<A>& beh) {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            boost::optional<light_ptr> ca = beh.get_constant_value();
            if (ca)
                return cell_<B>(f(ca.get()));
            else {
#endif
                auto impl = beh.impl;
                return map_<A,B>(trans, f, beh.updates_()).hold_lazy_(trans, [f, impl] () -> light_ptr {
                    return f(impl->sample());
                });
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            }
#endif
        }

        template <typename A>
        stream_<A> filter_optional_(transaction_impl* trans1, const stream_<boost::optional<A>>& input,
            const std::function<boost::optional<light_ptr>(const light_ptr&)>& f)
        {
            auto p = impl::unsafe_new_stream<A>();
            auto kill = input.listen_raw(trans1, std::get<1>(p),
                new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                    [f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& poa) {
                        boost::optional<light_ptr> oa = f(poa);
                        if (oa) impl::send(target, trans2, oa.get());
                    })
                , false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

    }  // namespace impl
}  // namespace sodium

#endif // _SODIUM_IMPL_H_
