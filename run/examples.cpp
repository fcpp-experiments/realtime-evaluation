// Copyright © 2025 Giorgio Audrito. All Rights Reserved.

/**
 * @file examples.cpp
 * @brief Experimental evaluation of real-time guarantees in FCPP.
 */

#include "lib/examples.hpp"

/**
 * @brief Namespace containing all the objects in the FCPP library.
 */
namespace fcpp {

//! @brief Number of nodes in the area.
constexpr int node_num = 400;
//! @brief Size of the area.
constexpr size_t size = 1000;
//! @brief The maximum communication range between nodes.
constexpr size_t comm_range = 100;
//! @brief Dimensionality of the space.
constexpr size_t dim = 2;

//! @brief Factor for calculating hues from real distances.
constexpr real_t hue_factor = 360.0 / size;

//! @brief End of the simulation.
constexpr size_t end_time = 200;
//! @brief Time after which old values are discarded.
constexpr times_t discard_time = size * 1.5 / comm_range;


//! @brief Namespace containing the libraries of coordination routines.
namespace coordination {

//! @brief Tags used in the node storage.
namespace tags {
    //! @brief Inner color band of the current node.
    struct node_color_in {};
    //! @brief Outer color bands of the current node.
    struct node_color_out {};
    //! @brief Size of the current node.
    struct node_size {};
    //! @brief Shape of the current node.
    struct node_shape {};
    //! @brief Value computed for the hop-count distance.
    struct hop_dist {};
    //! @brief Value computed for the hop-count diameter.
    struct hop_diam {};
    //! @brief Value computed for the stabilised real distance.
    struct stable_dist {};
    //! @brief Value computed for the stabilised real diameter.
    struct stable_diam {};
}

//! @brief Main function.
MAIN() {
    // import tag names in the local scope.
    using namespace tags;

    // change stable_diam source at the second half of the simulated time
    bool second_half = node.current_time()*4 > end_time;
    device_t hsource = second_half ? 200 : 100;

    // call the algorithms
    diam_data hd = hop_diameter(CALL, discard_time);
    diam_data sd = stable_diameter(CALL, hsource);

    // adjust hop-counts to be measurable as distances
    get<1>(hd) *= comm_range;
    get<2>(hd) *= comm_range;

    // display computed values in the storage
    node.storage(hop_dist{}) = get<1>(hd);
    node.storage(hop_diam{}) = get<2>(hd);
    node.storage(stable_dist{}) = get<1>(sd);
    node.storage(stable_diam{}) = get<2>(sd);
    node.storage(node_size{}) = get<0>(hd) or get<0>(sd) ? 20 : 12;
    node.storage(node_color_in{})  = color::hsva(get<1>(hd) * hue_factor, 1, 1);
    node.storage(node_color_out{}) = color::hsva(get<1>(sd) * hue_factor, 1, 1);
    node.storage(node_shape{}) = get<0>(sd) ? shape::octahedron : get<0>(hd) ? shape::cube : shape::sphere;

    // killing the hop_diam source at the second half of the simulated time
    if (node.uid == 0 and second_half and node.current_time() < end_time) {
        node.next_time(end_time+2);
        node.storage(node_color_in{}) = node.storage(node_color_out{}) = color(GRAY);
        node.storage(node_shape{}) = shape::icosahedron;
    }
}
//! @brief Export types used by the main function (update it when expanding the program).
FUN_EXPORT main_t = export_list<hop_diameter_t, stable_diameter_t>;

} // namespace coordination


// SYSTEM SETUP

//! @brief Namespace for component options.
namespace option {

//! @brief Import tags to be used for component options.
using namespace component::tags;
//! @brief Import tags used by aggregate functions.
using namespace coordination::tags;

//! @brief Description of the round schedule.
using round_s = sequence::periodic<
    distribution::interval_n<times_t, 0, 1>,      // uniform time in the [0,1] interval for start
    distribution::weibull_n<times_t, 10, 1, 10>,  // weibull-distributed time for interval (10/10=1 mean, 1/10=0.1 deviation)
    distribution::constant_n<times_t, end_time+2> // the constant end_time+2 number for end
>;
//! @brief The sequence of network snapshots (one every simulated second).
using log_s = sequence::periodic_n<1, 0, 1, end_time>;
//! @brief The sequence of node generation events (node_num devices all generated at time 0).
using spawn_s = sequence::multiple_n<node_num, 0>;
//! @brief The distribution of initial node positions (random in a square).
using rectangle_d = distribution::rect_n<1, 0, 0, size, size>;
//! @brief The contents of the node storage as tags and associated types.
using store_t = tuple_store<
    node_color_in,              color,
    node_color_out,             color,
    node_size,                  double,
    node_shape,                 shape,
    hop_dist,                   real_t,
    hop_diam,                   real_t,
    stable_dist,                real_t,
    stable_diam,                real_t,
    debug,                      std::string
>;
//! @brief The tags and corresponding aggregators to be logged (change as needed).
using aggregator_t = aggregators<
    hop_dist,                   aggregator::max<real_t>,
    stable_dist,                aggregator::max<real_t>,
    hop_diam,                   aggregator::combine<aggregator::min<real_t>, aggregator::max<real_t>>,
    stable_diam,                aggregator::combine<aggregator::min<real_t>, aggregator::max<real_t>>
>;

//! @brief The aggregator to be used on logging rows for plotting.
using row_aggregator_t = common::type_sequence<aggregator::mean<double>>;
//! @brief Combining the plots into a single row.
using plot_t = plot::split<plot::time, plot::values<aggregator_t, row_aggregator_t, hop_diam, stable_diam>>;

//! @brief The general simulation options.
DECLARE_OPTIONS(list,
    parallel<false>,      // multithreading enabled on node rounds
    synchronised<false>, // optimise for asynchronous networks
    program<coordination::main>,   // program to be run (refers to MAIN above)
    exports<coordination::main_t>, // export type list (types used in messages)
    retain<metric::retain<3,1>>,   // messages are kept for 3 seconds before expiring
    round_schedule<round_s>, // the sequence generator for round events on nodes
    log_schedule<log_s>,     // the sequence generator for log events on the network
    spawn_schedule<spawn_s>, // the sequence generator of node creation events on the network
    store_t,       // the contents of the node storage
    aggregator_t,  // the tags and corresponding aggregators to be logged
    plot_type<plot_t>, // the plot description to be used
    init<
        x,      rectangle_d // initialise position randomly in a rectangle for new nodes
    >,
    dimension<dim>, // dimensionality of the space
    connector<connect::fixed<comm_range, 1, dim>>, // connection allowed within a fixed comm range
    shape_tag<node_shape>, // the shape of a node is read from this tag in the store
    size_tag<node_size>,   // the size  of a node is read from this tag in the store
    color_tag<node_color_in, node_color_out> // node colors are read from these tags in the store
);

} // namespace option

} // namespace fcpp


//! @brief The main function.
int main() {
    using namespace fcpp;

    // The plotter object.
    option::plot_t p;
    std::cout << "/*\n";
    {
        // The network object type (interactive simulator with given options).
        using net_t = component::interactive_simulator<option::list>::net;
        // The initialisation values (simulation name and plotter object).
        auto init_v = common::make_tagged_tuple<option::name, option::plotter>("evaluation of real-time guarantees", &p);
        // Construct the network object.
        net_t network{init_v};
        // Run the simulation until exit.
        network.run();
    }
    // Build plots.
    std::cout << "*/\n";
    std::cout << plot::file("examples", p.build());
    return 0;
}
