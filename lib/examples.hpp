// Copyright © 2025 Giorgio Audrito. All Rights Reserved.

/**
 * @file examples.hpp
 * @brief FCPP code for the examples reported in the paper.
 *
 * This header file is designed to work under multiple execution paradigms.
 */

#ifndef FCPP_EXAMPLES_H_
#define FCPP_EXAMPLES_H_

//! Importing the FCPP library.
#include "lib/fcpp.hpp"


/**
 * @brief Namespace containing all the objects in the FCPP library.
 */
namespace fcpp {


//! @brief Namespace containing the libraries of coordination routines.
namespace coordination {

//! @brief Tags used in the node storage.
namespace tags {
    //! @brief String value for debugging.
    struct debug {};
}


//! @brief Auxiliary constants, types and non-distributed functions.
//! @{

//! @brief Maximum valid value for a hops_t variable.
constexpr hops_t HOPS_MAX = std::numeric_limits<hops_t>::max() - 1;

//! @brief A dictionary associating keys with timestamp to device IDs.
using time_dict = std::unordered_map<device_t, std::pair<times_t, real_t>>;

//! @brief Merges two time_dict by preferring the most recent values for each key.
time_dict update(time_dict x, time_dict const& y) {
    // updates values in x with more recent values in y
    for (auto& kv : x)
        if (y.count(kv.first) and y.at(kv.first).first > kv.second.first)
            kv.second = y.at(kv.first);
    // inserts in x new keys that are found in y
    for (auto const& kv : y)
        if (x.count(kv.first) == 0)
            x[kv.first] = kv.second;
    return x;
}

//! @brief Discards obsolete keys in a time_dict.
time_dict& discard(time_dict& x, times_t t) {
    for (auto it = x.begin(); it != x.end();)
        if (it->second.first < t) it = x.erase(it);
        else ++it;
    return x;
}

//! @brief Computes the maximum value in a time_dict.
real_t max_value(time_dict const& dict) {
    real_t val = 0;
    for (auto const& kv : dict)
        val = max(val, kv.second.second);
    return val;
}

//! @brief Computes the difference between the current and the previous time (1 for the first round).
template <typename node_t>
times_t delta_time(node_t const& node) {
    return node.previous_time() < 0 ? 1 : node.current_time() - node.previous_time();
}

//! @}


//! @brief Examples from Sections 5 and 6, translated into FCPP, ordered by their model class as in Table 1 (bottom to top).
//! @{

//! @brief Computes low-pass filtering of a real argument (SI-TI).
FUN real_t lowpass(ARGS, real_t v) { CODE
    return old(CALL, v, [&](real_t x){
        return (x+v)/2;
    });
}
//! @brief Export list for function lowpass.
FUN_EXPORT lowpass_t = export_list<real_t>;


//! @brief Integrates the values of the provided argument (SI-TC).
FUN real_t integrate(ARGS, real_t v) { CODE
    return old(CALL, 0, [&](real_t x){
        return x + v * delta_time(node);
    });
}
//! @brief Export list for function integrate.
FUN_EXPORT integrate_t = export_list<real_t>;


//! @brief Accumulates the values of the provided argument (SI-TD).
FUN real_t accumulate(ARGS, real_t v) { CODE
    return old(CALL, 0, [&](real_t x){
        return x + v;
    });
}
//! @brief Export list for function accumulate.
FUN_EXPORT accumulate_t = export_list<real_t>;


//! @brief Computes hop-count distances from the closest source device (SC-TI).
FUN real_t rdist(ARGS, bool source) { CODE
    return nbr(CALL, INF, [&](field<real_t> d){
        return mux(source, real_t(0), min_hood(CALL, d + node.nbr_dist(), INF));
    });
}
//! @brief Export list for function rdist.
FUN_EXPORT rdist_t = export_list<real_t>;


//! @brief Computes the maximum value of v in a network through timestamped gossiping (SC-TI).
FUN real_t maximize(ARGS, real_t v, times_t threshold) { CODE
    time_dict loc = {{node.uid, {node.current_time(),v}}};
    time_dict glob = nbr(CALL, loc, [&](field<time_dict> n){
        time_dict x = update(fold_hood(CALL, update, n), loc);
        return discard(x, node.current_time() - threshold);
    });
    return max_value(glob);
}
//! @brief Export list for function maximize.
FUN_EXPORT maximize_t = export_list<time_dict>;


//! @brief Computes the maximum value of v in the history of a network through basic gossiping (SC-TC).
FUN real_t maxgossip(ARGS, real_t v) { CODE
    return nbr(CALL, v, [&](field<real_t> n){
        return max(max_hood(CALL, n), v);
    });
}
//! @brief Export list for function maxgossip.
FUN_EXPORT maxgossip_t = export_list<real_t>;


//! @brief Computes hop-count distances from the closest source device (SD-TI).
FUN hops_t dist(ARGS, bool source) { CODE
    return nbr(CALL, HOPS_MAX, [&](field<hops_t> d){
        return (hops_t)mux(source, 0, min_hood(CALL, d, HOPS_MAX) + 1);
    });
}
//! @brief Export list for function dist.
FUN_EXPORT dist_t = export_list<hops_t>;


//! @brief Knowledge-free leader election as in Mo et al. (SD-TI).
FUN bool election(ARGS) { CODE
    // already implemented in the FCPP coordination library
    return wave_election(CALL) == node.uid;
}
//! @brief Export list for function election.
FUN_EXPORT election_t = export_list<wave_election_t<>>;


//! @brief Implementation of the SLCS formula `a R (<>b)` (SD-TI).
FUN bool closereach(ARGS, bool a, bool b) { CODE
    // SCLS as implemented in the FCPP coordination library
    using namespace logic;
    return R(CALL, a, C(CALL, b));
}
//! @brief Export list for function closereach.
FUN_EXPORT closereach_t = export_list<slcs_t>;


//! @brief Integrates the values of the provided argument (SD-TC).
FUN bool minintegral(ARGS, real_t v) { CODE
    real_t i = integrate(CALL, v);
    return i < min_hood(CALL, nbr(CALL, i), INF);
}
//! @brief Export list for function minintegral.
FUN_EXPORT minintegral_t = export_list<integrate_t, real_t>;


//! @brief Computes a counter that is collaboratively increased across the network (SD-TD).
FUN int sharedcount(ARGS) { CODE
    return nbr(CALL, 0, [&](field<int> n){
        return max_hood(CALL, n)+1;
    });
}
//! @brief Export list for function sharedcounter.
FUN_EXPORT sharedcounter_t = export_list<int>;

//! @}


//! @brief Case studies from Section 7, translated into FCPP.
//! @{

//! @brief Structure holding data representing a diameter calculation (source, distance, diameter).
using diam_data = tuple<bool, real_t, real_t>;


/**
 * @brief Calculates the diameter of a network.
 *
 * Function in SD-TI, with Specification 1 (minimal) at T(I) = (4+2√2)Dt + threshold.
 */
FUN diam_data hop_diameter(ARGS, times_t threshold) { CODE
    bool source = election(CALL);
    hops_t d = dist(CALL, source);
    real_t diam = maximize(CALL, d, threshold);
    return diam_data(source, d, diam);
}
//! @brief Export list for function hop_diameter.
FUN_EXPORT hop_diameter_t = export_list<election_t, dist_t, maximize_t>;


/**
 * @brief Stabilised calculation of the diameter of a network.
 *
 * Function in SC-TC, that could comply to a form of Specification 4 (continuous).
 */
FUN diam_data stable_diameter(ARGS, bool source) { CODE
    real_t d = rdist(CALL, source);
    real_t z = d == INF ? 0 : d;
    real_t avgd = integrate(CALL, z) / integrate(CALL, 1);
    real_t diam = maxgossip(CALL, lowpass(CALL, avgd));
    return diam_data(source, avgd, diam);
}
//! @brief Export list for function stable_diameter.
FUN_EXPORT stable_diameter_t = export_list<rdist_t, integrate_t, lowpass_t, maxgossip_t>;

//! @}


} // namespace coordination


} // namespace fcpp


#endif // FCPP_EXAMPLES_H_
