#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/size.hpp>

// For printing
#include <boost/hana/for_each.hpp>
#include <boost/type_index.hpp>
#include <iostream>


#include <cassert> // TODO: For testing.
#define BOOST_PROTO17_STREAM_OPERATORS // TODO: For testing.

// TODO: Verbose debugging mode for matching.
// TODO: Proto-style "Fuzzy and Exact Matches of Terminals".

namespace boost::proto17 {

    enum class expr_kind {
        terminal,
        placeholder,

        plus,
        minus,

        // etc...
    };

    template <expr_kind Kind, typename ...T>
    struct expression;

    template <typename T>
    using terminal = expression<expr_kind::terminal, T>;

#define BOOST_PROTO17_NOEXCEPT_DECLTYPE_RETURN(expr)                    \
        noexcept(noexcept(expr)) -> decltype(expr) { return expr; }

    namespace adl_detail {

        template <typename T, typename U>
        constexpr auto eval_plus (T && t, U && u) BOOST_PROTO17_NOEXCEPT_DECLTYPE_RETURN(
            static_cast<T &&>(t) + static_cast<U &&>(u)
        )

        struct eval_plus_fn
        {
            template <typename T, typename U>
            constexpr auto operator() (T && t, U && u) const BOOST_PROTO17_NOEXCEPT_DECLTYPE_RETURN(
                eval_plus(static_cast<T &&>(t), static_cast<U &&>(u))
            )
        };

        template <typename T, typename U>
        constexpr auto eval_minus (T && t, U && u) BOOST_PROTO17_NOEXCEPT_DECLTYPE_RETURN(
            static_cast<T &&>(t) - static_cast<U &&>(u)
        )

        struct eval_minus_fn
        {
            template <typename T, typename U>
            constexpr auto operator() (T && t, U && u) const BOOST_PROTO17_NOEXCEPT_DECLTYPE_RETURN(
                eval_minus(static_cast<T &&>(t), static_cast<U &&>(u))
            )
        };

    }

#undef BOOST_PROTO17_NOEXCEPT_DECLTYPE_RETURN

    using adl_detail::eval_plus_fn;
    using adl_detail::eval_minus_fn;

    inline namespace function_objects {

        inline constexpr eval_plus_fn eval_plus{};
        inline constexpr eval_minus_fn eval_minus{};

    }

    namespace detail {

        template <typename T>
        struct partial_decay
        {
            using type = T;
        };

        template <typename T>
        struct partial_decay<T[]> { using type = T *; };
        template <typename T, std::size_t N>
        struct partial_decay<T[N]> { using type = T *; };

        template <typename T>
        struct partial_decay<T(&)[]> { using type = T *; };
        template <typename T, std::size_t N>
        struct partial_decay<T(&)[N]> { using type = T *; };

        template <typename R, typename ...A>
        struct partial_decay<R(A...)> { using type = R(*)(A...); };
        template <typename R, typename ...A>
        struct partial_decay<R(A..., ...)> { using type = R(*)(A..., ...); };

        template <typename T,
                  typename U = typename detail::partial_decay<T>::type,
                  bool AddRValueRef = std::is_same_v<T, U> && !std::is_const_v<U>>
        struct rhs_value_type_phase_1;

        template <typename T, typename U>
        struct rhs_value_type_phase_1<T, U, true>
        { using type = U &&; };

        template <typename T, typename U>
        struct rhs_value_type_phase_1<T, U, false>
        { using type = U; };

        template <typename ...T>
        struct is_expr
        { static bool const value = false; };

        template <expr_kind Kind, typename ...T>
        struct is_expr<expression<Kind, T...>>
        { static bool const value = true; };

        template <typename T,
                  typename U = typename rhs_value_type_phase_1<T>::type,
                  bool RemoveRefs = std::is_rvalue_reference_v<U>,
                  bool IsExpr = is_expr<std::decay_t<T>>::value>
        struct rhs_type;

        template <typename T, typename U, bool RemoveRefs>
        struct rhs_type<T, U, RemoveRefs, true>
        { using type = std::remove_cv_t<std::remove_reference_t<T>>; };

        template <typename T, typename U>
        struct rhs_type<T, U, true, false>
        { using type = terminal<std::remove_reference_t<U>>; };

        template <typename T, typename U>
        struct rhs_type<T, U, false, false>
        { using type = terminal<U>; };

        template <typename Tuple, expr_kind Kind, typename ...T>
        auto default_eval_expr (expression<Kind, T...> const & expr, Tuple && tuple)
        {
            using namespace hana::literals;
            if constexpr (Kind == expr_kind::terminal) {
                static_assert(sizeof...(T) == 1);
                return expr.elements[0_c];
            } else if constexpr (Kind == expr_kind::placeholder) {
                static_assert(sizeof...(T) == 1);
                return tuple[expr.elements[0_c]];
            } else if constexpr (Kind == expr_kind::plus) {
                return
                    eval_plus(
                        default_eval_expr(expr.elements[0_c], static_cast<Tuple &&>(tuple)),
                        default_eval_expr(expr.elements[1_c], static_cast<Tuple &&>(tuple))
                    );
            } else if constexpr (Kind == expr_kind::minus) {
                return
                    eval_minus(
                        default_eval_expr(expr.elements[0_c], static_cast<Tuple &&>(tuple)),
                        default_eval_expr(expr.elements[1_c], static_cast<Tuple &&>(tuple))
                    );
            } else {
                assert(false && "Unhandled expr_kind in default_evaluate!");
                return;
            }
        }

    }

    // TODO: Customization point.
    // TODO: static assert/SFINAE std::is_callable<>
    // TODO: static assert/SFINAE no placeholders
    template <typename R, expr_kind Kind, typename ...T>
    R evaluate_expression_as (expression<Kind, T...> const & expr)
    { return static_cast<R>(detail::default_eval_expr(expr, hana::tuple<>())); }

    // TODO: static assert/SFINAE std::is_callable<>
    template <typename Expr, typename ...T>
    auto evaluate (Expr const & expr, T && ...t)
    { return detail::default_eval_expr(expr, hana::make_tuple(static_cast<T &&>(t)...)); }

    template <expr_kind Kind, typename ...T>
    struct expression
    {
        using this_type = expression<Kind, T...>;
        using tuple_type = hana::tuple<T...>;

        static const expr_kind kind = Kind;

        expression (T && ... t) :
            elements (static_cast<T &&>(t)...)
        {}

        expression (hana::tuple<T...> const & rhs) :
            elements (rhs)
        {}

        expression (hana::tuple<T...> && rhs) :
            elements (std::move(rhs))
        {}

        expression & operator= (hana::tuple<T...> const & rhs)
        { elements = rhs.elements; }

        expression & operator= (hana::tuple<T...> && rhs)
        { elements = std::move(rhs.elements); }

        tuple_type elements;

        template <typename R>
        operator R ()
        { return evaluate_expression_as<R>(*this); }

        template <typename U>
        auto operator+ (U && rhs) const &
        {
            using rhs_type = typename detail::rhs_type<U>::type;
            return expression<expr_kind::plus, this_type, rhs_type>{
                hana::tuple<this_type, rhs_type>{*this, rhs_type{static_cast<U &&>(rhs)}}
            };
        }

        template <typename U>
        auto operator+ (U && rhs) &&
        {
            using rhs_type = typename detail::rhs_type<U>::type;
            return expression<expr_kind::plus, this_type, rhs_type>{
                hana::tuple<this_type, rhs_type>{std::move(*this), rhs_type{static_cast<U &&>(rhs)}}
            };
        }

        template <typename U>
        auto operator- (U && rhs) const &
        {
            using rhs_type = typename detail::rhs_type<U>::type;
            return expression<expr_kind::minus, this_type, rhs_type>{
                hana::tuple<this_type, rhs_type>{*this, rhs_type{static_cast<U &&>(rhs)}}
            };
        }

        template <typename U>
        auto operator- (U && rhs) &&
        {
            using rhs_type = typename detail::rhs_type<U>::type;
            return expression<expr_kind::minus, this_type, rhs_type>{
                hana::tuple<this_type, rhs_type>{std::move(*this), rhs_type{static_cast<U &&>(rhs)}}
            };
        }
    };

    template <long long I>
    using placeholder = expression<expr_kind::placeholder, hana::llong<I>>;

    namespace literals {

        template <char ...c>
        constexpr auto operator"" _p ()
        {
            using i = hana::llong<hana::ic_detail::parse<sizeof...(c)>({c...})>;
            return expression<expr_kind::placeholder, i>(i{});
        }

    }

    namespace match {

        template <typename KindMatches, typename ...T>
        struct expression
        {
            KindMatches kind_matches;
            hana::tuple<T...> elements;
        };

        // TODO: Use this to detect identical types within a match.
        template <long long I>
        struct placeholder : hana::llong<I> {};

    }

    template <typename KindMatches, typename T>
    constexpr bool is_match_expression (hana::basic_type<T>)
    { return false; }
    template <typename KindMatches, typename ...T>
    constexpr bool is_match_expression (hana::basic_type<match::expression<KindMatches, T...>>)
    { return true; }

    namespace literals {

        template <char ...c>
        constexpr auto operator"" _T ()
        { return match::placeholder<hana::ic_detail::parse<sizeof...(c)>({c...})>{}; }

    }

    namespace detail {

        inline std::ostream & print_kind (std::ostream & os, expr_kind kind)
        {
            switch (kind) {
            case expr_kind::plus: return os << "+";
            case expr_kind::minus: return os << "-";
            // TODO
            default: return os << "** ERROR: UNKNOWN OPERATOR! **";
            }
        }

        template <typename T>
        auto print_value (std::ostream & os, T const & x) -> decltype(os << x)
        { return os << x; }

        inline std::ostream & print_value (std::ostream & os, ...)
        { return os << "<<unprintable-value>>"; }

        template <typename T>
        std::ostream & print_type (std::ostream & os)
        {
            os << typeindex::type_id<T>().pretty_name();
            if (std::is_const_v<T>)
                os << " const";
            if (std::is_volatile_v<T>)
                os << " volatile";
            using no_cv_t = std::remove_cv_t<T>;
            if (std::is_lvalue_reference_v<T>)
                os << " &";
            if (std::is_rvalue_reference_v<T>)
                os << " &&";
            return os;
        }

        template <expr_kind Kind, typename T, typename ...Ts>
        std::ostream & print_impl (
            std::ostream & os,
            expression<Kind, T, Ts...> const & expr,
            int indent,
            char const * indent_str)
        {
            for (int i = 0; i < indent; ++i) {
                os << indent_str;
            }

            if constexpr (Kind == expr_kind::terminal) {
                using namespace hana::literals;
                static_assert(sizeof...(Ts) == 0);
                os << "term<";
                print_type<T>(os);
                os << ">[=";
                print_value(os, expr.elements[0_c]);
                os << "]\n";
            } else if constexpr (Kind == expr_kind::placeholder) {
                using namespace hana::literals;
                static_assert(sizeof...(Ts) == 0);
                os << "placeholder<" << (long long)expr.elements[0_c] << ">\n";
            } else {
                os << "expr<";
                print_kind(os, Kind);
                os << ">\n";
                hana::for_each(expr.elements, [&os, indent, indent_str](auto const & element) {
                    print_impl(os, element, indent + 1, indent_str);
                });
            }

            return os;
        }

    }

    template <typename T>
    std::ostream & print (std::ostream & os, terminal<T> const & term)
    { return detail::print_impl(os, term, 0, "    "); }

    template <expr_kind Kind, typename ...T>
    std::ostream & print (std::ostream & os, expression<Kind, T...> const & expr)
    { return detail::print_impl(os, expr, 0, "    "); }

#if defined(BOOST_PROTO17_STREAM_OPERATORS)
    template <typename T>
    std::ostream & operator<< (std::ostream & os, terminal<T> const & term)
    { return detail::print_impl(os, term, 0, "    "); }

    template <expr_kind Kind, typename ...T>
    std::ostream & operator<< (std::ostream & os, expression<Kind, T...> const & expr)
    { return detail::print_impl(os, expr, 0, "    "); }
#endif

    namespace detail {

        inline bool matches (...)
        { return false; }

        template <typename MatchExpr, typename TreeExpr>
        bool matches (MatchExpr const & match_subtree, TreeExpr const & subtree);

        template <std::size_t I, typename MatchExpr, typename TreeExpr>
        void recursive_match_impl (MatchExpr const & match_subtree, TreeExpr const & subtree, bool & result)
        {
            if (!matches(match_subtree[hana::size_c<I>], subtree[hana::size_c<I>])) {
                result = false;
            }
            if constexpr (0 < I) {
                recursive_match_impl<I - 1>(match_subtree, subtree, result);
            }
        }

        template <typename MatchExpr, typename TreeExpr>
        bool matches (MatchExpr const & match_subtree, TreeExpr const & subtree)
        {
            static_assert(is_match_expression(hana::typeid_(match_subtree)),
                          "Attempted to use a non-tree as a match expression.");
            static_assert(is_expression(hana::typeid_(subtree)),
                          "Attempted to use find a match in a non-tree.");

            // TODO: Verbose mode.

            if (!match_subtree.kind_matches(subtree)) {
                return false;
            } else {
                auto constexpr subtree_size = hana::size(subtree.elements);
                if constexpr (hana::size(match_subtree.elements) != subtree_size) {
                    // TODO: Verbose mode.
                    return false;
                } else {
                    bool children_match = true;
                    recursive_match_impl<subtree_size - 1>(
                        match_subtree.elements,
                        subtree.elements,
                        children_match
                    );
                    return children_match;
                }
            }
        }

        template <typename Matcher, typename Callable, expr_kind Kind, typename ...T>
        auto mutate_subtrees_of (
            Matcher const & match_subtree,
            expression<Kind, T...> & tree,
            Callable && mutation
        ) {
            // TODO: Process children first.
            if (matches(match_subtree, tree)) {
                return mutation(tree);
            } else if (Kind == expr_kind::terminal) {
                return tree;
            } else {
                auto mutate_child = [&match_subtree, &mutation] (auto & t) {
                    return mutate_subtrees_of(match_subtree, t, static_cast<Callable &&>(mutation));
                };
                auto return_elements = hana::transform(tree.elements, mutate_child);
                return make_expression(Kind, std::move(return_elements));
            }
        }

    }

}


#include <string>


template <typename T>
using term = boost::proto17::terminal<T>;

namespace bp17 = boost::proto17;
using namespace std::string_literals;


void term_plus_x ()
{
    // char const * string
    {
        term<double> unity{1.0};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<char const *>
        > unevaluated_expr = unity + "3";
    }

    // std::string temporary
    {
        term<double> unity{1.0};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<std::string>
        > unevaluated_expr = unity + "3"s;
    }

    // arrays
    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int *>
        > unevaluated_expr = unity + ints;
    }

    {
        term<double> unity{1.0};
        int const ints[] = {1, 2};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const *>
        > unevaluated_expr = unity + ints;
    }

    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int *>
        > unevaluated_expr = unity + std::move(ints);
    }

    // pointers
    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        int * int_ptr = ints;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int * &>
        > unevaluated_expr = unity + int_ptr;
    }

    {
        term<double> unity{1.0};
        int const ints[] = {1, 2};
        int const * int_ptr = ints;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const * &>
        > unevaluated_expr = unity + int_ptr;
    }

    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        int * int_ptr = ints;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int *>
        > unevaluated_expr = unity + std::move(int_ptr);
    }

    // const pointers
    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        int * const int_ptr = ints;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int * const &>
        > unevaluated_expr = unity + int_ptr;
    }

    {
        term<double> unity{1.0};
        int const ints[] = {1, 2};
        int const * const int_ptr = ints;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const * const &>
        > unevaluated_expr = unity + int_ptr;
    }

    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        int * const int_ptr = ints;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int * const>
        > unevaluated_expr = unity + std::move(int_ptr);
    }

    // values
    {
        term<double> unity{1.0};
        int i = 1;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> unity{1.0};
        int const i = 1;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const &>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> unity{1.0};
        int i = 1;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int>
        > unevaluated_expr = unity + std::move(i);
    }
}

void term_plus_x_this_ref_overloads()
{
    {
        term<double> unity{1.0};
        int i = 1;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> const unity{1.0};
        int i = 1;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> unity{1.0};
        int i = 1;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > unevaluated_expr = std::move(unity) + i;
    }
}

void term_plus_term ()
{
    // char const * string
    {
        term<double> unity{1.0};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<char const *>
        > unevaluated_expr = unity + term<char const *>{"3"};
    }

    // std::string temporary
    {
        term<double> unity{1.0};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<std::string>
        > unevaluated_expr = unity + term<std::string>{"3"s};
    }

    // pointers
    {
        term<double> unity{1.0};
        int ints_[] = {1, 2};
        term<int *> ints = {ints_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int *>
        > unevaluated_expr = unity + ints;
    }

    {
        term<double> unity{1.0};
        int const ints_[] = {1, 2};
        term<int const *> ints = {ints_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const *>
        > unevaluated_expr = unity + ints;
    }

    {
        term<double> unity{1.0};
        int ints_[] = {1, 2};
        term<int *> ints = {ints_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int *>
        > unevaluated_expr = unity + std::move(ints);
    }

    // const pointers
    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        term<int * const> int_ptr = {ints};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int * const>
        > unevaluated_expr = unity + int_ptr;
    }

    {
        term<double> unity{1.0};
        int const ints[] = {1, 2};
        term<int const * const> int_ptr = {ints};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const * const>
        > unevaluated_expr = unity + int_ptr;
    }

    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        term<int * const> int_ptr = {ints};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int * const>
        > unevaluated_expr = unity + std::move(int_ptr);
    }

    // values
    {
        term<double> unity{1.0};
        term<int> i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> unity{1.0};
        term<int const> i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> unity{1.0};
        term<int> i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int>
        > unevaluated_expr = unity + std::move(i);
    }

    // const value terminals
    {
        term<double> unity{1.0};
        term<int> const i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> unity{1.0};
        term<int const> const i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const>
        > unevaluated_expr = unity + i;
    }

    // lvalue refs
    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int const &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const &>
        > unevaluated_expr = unity + i;
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > unevaluated_expr = unity + std::move(i);
    }

    // rvalue refs
    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &&> i{std::move(i_)};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &&>
        > unevaluated_expr = unity + std::move(i);
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &&> i{std::move(i_)};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &&>
        > unevaluated_expr = unity + std::move(i);
    }
}

void term_plus_expr ()
{
    // values
    {
        term<double> unity{1.0};
        term<int> i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int>
        > expr = unity + i;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int>
            >
        > unevaluated_expr = unity + expr;
    }

    {
        term<double> unity{1.0};
        term<int const> i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const>
        > expr = unity + i;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int const>
            >
        > unevaluated_expr = unity + expr;
    }

    {
        term<double> unity{1.0};
        term<int> i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int>
            >
        > unevaluated_expr = unity + expr;
    }

    // const value terminals/expressions
    {
        term<double> unity{1.0};
        term<int> const i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int>
        > const expr = unity + i;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int>
            >
        > unevaluated_expr = unity + expr;
    }

    {
        term<double> unity{1.0};
        term<int> i = {1};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int>
        > const expr = unity + i;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int>
            >
        > unevaluated_expr = unity + expr;
    }

    // lvalue refs
    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > expr = unity + i;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &>
            >
        > unevaluated_expr = unity + expr;
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int const &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const &>
        > expr = unity + i;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int const &>
            >
        > unevaluated_expr = unity + expr;
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &>
            >
        > unevaluated_expr = unity + expr;
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > expr = unity + i;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &>
            >
        > unevaluated_expr = unity + std::move(expr);
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int const &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const &>
        > expr = unity + i;
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int const &>
            >
        > unevaluated_expr = unity + std::move(expr);
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &>
            >
        > unevaluated_expr = unity + std::move(expr);
    }

    // rvalue refs
    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &&> i{std::move(i_)};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &&>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &&>
            >
        > unevaluated_expr = unity + std::move(expr);
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &&> i{std::move(i_)};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &&>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &&>
            >
        > unevaluated_expr = unity + std::move(expr);
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &&> i{std::move(i_)};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &&>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &&>
            >
        > unevaluated_expr = unity + std::move(expr);
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &&> i{std::move(i_)};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &&>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &&>
            >
        > unevaluated_expr = unity + std::move(expr);
    }
}

void placeholders ()
{
    using namespace boost::proto17::literals;

    {
        bp17::placeholder<0> p0 = 0_p;
    }

    {
        bp17::placeholder<0> p0 = 0_p;
        term<double> unity{1.0};
        bp17::expression<
            bp17::expr_kind::plus,
            bp17::placeholder<0>,
            term<double>
        > expr = p0 + unity;
    }

    {
        bp17::placeholder<0> p0 = 0_p;
        bp17::expression<
            bp17::expr_kind::plus,
            bp17::placeholder<0>,
            bp17::placeholder<1>
        > expr = p0 + 1_p;
    }
}

void const_term_expr ()
{
    {
        term<double const> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double const>,
            term<int &&>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double const>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double const>,
                term<int &&>
            >
        > unevaluated_expr = unity + std::move(expr);
    }

    {
        term<double> const unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &&>
        > expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int &&>
            >
        > unevaluated_expr = unity + std::move(expr);
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int const &> i{i_};
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int const &>
        > const expr = unity + std::move(i);
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            bp17::expression<
                bp17::expr_kind::plus,
                term<double>,
                term<int const &>
            >
        > unevaluated_expr = unity + std::move(expr);
    }
}

void print ()
{
    term<double> unity{1.0};
    int i_ = 42;
    term<int &&> i{std::move(i_)};
    bp17::expression<
        bp17::expr_kind::plus,
        term<double>,
        term<int &&>
    > expr = unity + std::move(i);
    bp17::expression<
        bp17::expr_kind::plus,
        term<double>,
        bp17::expression<
            bp17::expr_kind::plus,
            term<double>,
            term<int &&>
        >
    > unevaluated_expr = unity + std::move(expr);

    std::cout << "================================================================================\n";
    bp17::print(std::cout, unity);
    std::cout << "================================================================================\n";
    bp17::print(std::cout, expr);
    std::cout << "================================================================================\n";
    bp17::print(std::cout, unevaluated_expr);

    struct thing {};
    term<thing> a_thing(thing{});
    std::cout << "================================================================================\n";
    bp17::print(std::cout, a_thing);

    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "================================================================================\n";
    {
        using namespace boost::proto17::literals;
        std::cout << (0_p + unity);
        std::cout << (2_p + 3_p);
        std::cout << (unity + 1_p);
    }

#if defined(BOOST_PROTO17_STREAM_OPERATORS)
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "================================================================================\n";
    std::cout << unity;
    std::cout << "================================================================================\n";
    std::cout << expr;
    std::cout << "================================================================================\n";
    std::cout << unevaluated_expr;
    std::cout << "================================================================================\n";
    std::cout << a_thing;
#endif
}

void default_eval ()
{
    term<double> unity{1.0};
    int i_ = 42;
    term<int &&> i{std::move(i_)};
    bp17::expression<
        bp17::expr_kind::minus,
        term<double>,
        term<int &&>
    > expr = unity - std::move(i);
    bp17::expression<
        bp17::expr_kind::plus,
        term<double>,
        bp17::expression<
            bp17::expr_kind::minus,
            term<double>,
            term<int &&>
        >
    > unevaluated_expr = unity + std::move(expr);

    {
        double result = unity;
        std::cout << "unity=" << result << "\n"; // 1
    }

    {
        double result = expr;
        std::cout << "expr=" << result << "\n"; // -41
    }

    {
        double result = unevaluated_expr;
        std::cout << "unevaluated_expr=" << result << "\n"; // -40
    }

    {
        double result = evaluate(unity, boost::hana::make_tuple(5, 6, 7));
        std::cout << "evaluate(unity)=" << result << "\n"; // 1
    }

    {
        double result = evaluate(expr, boost::hana::make_tuple());
        std::cout << "evaluate(expr)=" << result << "\n"; // -41
    }

    {
        double result = evaluate(unevaluated_expr, boost::hana::make_tuple(std::string("15")));
        std::cout << "evaluate(unevaluated_expr)=" << result << "\n"; // -40
    }
}

namespace test {

    struct number
    {
        explicit operator double () const { return value; }

        double value;
    };

    // User-defined binary-plus!  With weird semantics!
    inline auto eval_plus (number a, number b)
    { return number{a.value - b.value}; }

}

void user_operator_eval ()
{
    term<test::number> unity{{1.0}};
    double d_ = 42.0;
    term<test::number> i{{d_}};
    bp17::expression<
        bp17::expr_kind::plus,
        term<test::number>,
        term<test::number>
    > expr = unity + std::move(i);
    bp17::expression<
        bp17::expr_kind::plus,
        term<test::number>,
        bp17::expression<
            bp17::expr_kind::plus,
            term<test::number>,
            term<test::number>
        >
    > unevaluated_expr = unity + std::move(expr);

    {
        double result = unity;
        std::cout << "unity=" << result << "\n"; // 1
    }

    {
        double result = expr;
        std::cout << "expr=" << result << "\n"; // -41
    }

    {
        double result = unevaluated_expr;
        std::cout << "unevaluated_expr=" << result << "\n"; // 42
    }

    {
        double result = (double)evaluate(unity, boost::hana::make_tuple(5, 6, 7));
        std::cout << "evaluate(unity)=" << result << "\n"; // 1
    }

    {
        double result = (double)evaluate(expr, boost::hana::make_tuple());
        std::cout << "evaluate(expr)=" << result << "\n"; // -41
    }

    {
        double result = (double)evaluate(unevaluated_expr, boost::hana::make_tuple(std::string("15")));
        std::cout << "evaluate(unevaluated_expr)=" << result << "\n"; // 42
    }
}

void placeholder_eval ()
{
    using namespace boost::proto17::literals;

    bp17::placeholder<2> p2 = 2_p;
    int i_ = 42;
    term<int> i{std::move(i_)};
    bp17::expression<
        bp17::expr_kind::plus,
        bp17::placeholder<2>,
        term<int>
    > expr = p2 + std::move(i);
    bp17::expression<
        bp17::expr_kind::plus,
        bp17::placeholder<2>,
        bp17::expression<
            bp17::expr_kind::plus,
            bp17::placeholder<2>,
            term<int>
        >
    > unevaluated_expr = p2 + std::move(expr);

    {
        double result = evaluate(p2, 5, 6, 7);
        std::cout << "evaluate(p2)=" << result << "\n"; // 7
    }

    {
        double result = evaluate(expr, std::string("15"), 3, 1);
        std::cout << "evaluate(expr)=" << result << "\n"; // 43
    }

    {
        double result = evaluate(unevaluated_expr, std::string("15"), 2, 3);
        std::cout << "evaluate(unevaluated_expr)=" << result << "\n"; // 48
    }
}

int main ()
{
    term_plus_x();
    term_plus_x_this_ref_overloads();
    term_plus_term();
    term_plus_expr();
    placeholders();

    const_term_expr();

    print();

    default_eval();
    user_operator_eval();

    placeholder_eval();

#if 0 // TODO
    {
        bp17::terminal<double> unity{1.0};

        auto unevaluated_expr = unity + "3";
        auto mutated_expr = mutate(unevaluated_expr, match_expr, mutation);
        auto result = bp17::eval(unevaluated_expr);
    }

    {
        bp17::terminal<double> a{1.0};
        bp17::terminal<double> x{2.0};
        bp17::terminal<int> b{3};

        auto unevaluated_expr = a * x + b;
        auto match_expr = 0_T * 0_T + 1_T;
        auto mutated_expr = mutate(unevaluated_expr, match_expr, mutation);
        auto result = bp17::eval(mutated_expr);
    }

    {
        bp17::terminal<double> a{1.0};
        bp17::terminal<double> x{2.0};
        bp17::terminal<int> b{3};

        auto match_double_2 = [] (auto && terminal) {
            if constexpr (hana::typeid_(terminal) == hana::type<double>{})
                return terminal == 2.0;
            else
                return false;
        };

        auto unevaluated_expr = a * x + b;
        auto match_expr = bp17::match<double> * match_double_2 + 0_T;
        auto mutated_expr = mutate(unevaluated_expr, match_expr, mutation);
        auto result = bp17::eval(mutated_expr);
    }
#endif
}
