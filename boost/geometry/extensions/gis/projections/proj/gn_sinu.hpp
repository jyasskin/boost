#ifndef BOOST_GEOMETRY_PROJECTIONS_GN_SINU_HPP
#define BOOST_GEOMETRY_PROJECTIONS_GN_SINU_HPP

// Boost.Geometry - extensions-gis-projections (based on PROJ4)
// This file is automatically generated. DO NOT EDIT.

// Copyright (c) 2008-2012 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This file is converted from PROJ4, http://trac.osgeo.org/proj
// PROJ4 is originally written by Gerald Evenden (then of the USGS)
// PROJ4 is maintained by Frank Warmerdam
// PROJ4 is converted to Boost.Geometry by Barend Gehrels

// Original copyright notice:
 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include <boost/concept_check.hpp>
#include <boost/math/special_functions/hypot.hpp>

#include <boost/geometry/extensions/gis/projections/impl/base_static.hpp>
#include <boost/geometry/extensions/gis/projections/impl/base_dynamic.hpp>
#include <boost/geometry/extensions/gis/projections/impl/projects.hpp>
#include <boost/geometry/extensions/gis/projections/impl/factory_entry.hpp>
#include <boost/geometry/extensions/gis/projections/impl/pj_mlfn.hpp>

namespace boost { namespace geometry { namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace gn_sinu{ 
            static const double EPS10 = 1e-10;
            static const int MAX_ITER = 8;
            static const double LOOP_TOL = 1e-7;

            struct par_gn_sinu
            {
                double    en[EN_SIZE];
                double    m, n, C_x, C_y;
            };
            /* Ellipsoidal Sinusoidal only */

            // template class, using CRTP to implement forward/inverse
            template <typename Geographic, typename Cartesian, typename Parameters>
            struct base_gn_sinu_ellipsoid : public base_t_fi<base_gn_sinu_ellipsoid<Geographic, Cartesian, Parameters>,
                     Geographic, Cartesian, Parameters>
            {

                 typedef double geographic_type;
                 typedef double cartesian_type;

                par_gn_sinu m_proj_parm;

                inline base_gn_sinu_ellipsoid(const Parameters& par)
                    : base_t_fi<base_gn_sinu_ellipsoid<Geographic, Cartesian, Parameters>,
                     Geographic, Cartesian, Parameters>(*this, par) {}

                inline void fwd(geographic_type& lp_lon, geographic_type& lp_lat, cartesian_type& xy_x, cartesian_type& xy_y) const
                {
                    double s, c;
                
                    xy_y = pj_mlfn(lp_lat, s = sin(lp_lat), c = cos(lp_lat), this->m_proj_parm.en);
                    xy_x = lp_lon * c / sqrt(1. - this->m_par.es * s * s);
                }

                inline void inv(cartesian_type& xy_x, cartesian_type& xy_y, geographic_type& lp_lon, geographic_type& lp_lat) const
                {
                    double s;
                
                    if ((s = fabs(lp_lat = pj_inv_mlfn(xy_y, this->m_par.es, this->m_proj_parm.en))) < HALFPI) {
                        s = sin(lp_lat);
                        lp_lon = xy_x * sqrt(1. - this->m_par.es * s * s) / cos(lp_lat);
                    } else if ((s - EPS10) < HALFPI)
                        lp_lon = 0.;
                    else throw proj_exception();;
                            return;
                }
                /* General spherical sinusoidals */
            };

            // template class, using CRTP to implement forward/inverse
            template <typename Geographic, typename Cartesian, typename Parameters>
            struct base_gn_sinu_spheroid : public base_t_fi<base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>,
                     Geographic, Cartesian, Parameters>
            {

                 typedef double geographic_type;
                 typedef double cartesian_type;

                par_gn_sinu m_proj_parm;

                inline base_gn_sinu_spheroid(const Parameters& par)
                    : base_t_fi<base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>,
                     Geographic, Cartesian, Parameters>(*this, par) {}

                inline void fwd(geographic_type& lp_lon, geographic_type& lp_lat, cartesian_type& xy_x, cartesian_type& xy_y) const
                {
                    if (!this->m_proj_parm.m)
                        lp_lat = this->m_proj_parm.n != 1. ? aasin(this->m_proj_parm.n * sin(lp_lat)): lp_lat;
                    else {
                        double k, V;
                        int i;
                
                        k = this->m_proj_parm.n * sin(lp_lat);
                        for (i = MAX_ITER; i ; --i) {
                            lp_lat -= V = (this->m_proj_parm.m * lp_lat + sin(lp_lat) - k) /
                                (this->m_proj_parm.m + cos(lp_lat));
                            if (fabs(V) < LOOP_TOL)
                                break;
                        }
                        if (!i)
                            throw proj_exception();
                    }
                    xy_x = this->m_proj_parm.C_x * lp_lon * (this->m_proj_parm.m + cos(lp_lat));
                    xy_y = this->m_proj_parm.C_y * lp_lat;
                }

                inline void inv(cartesian_type& xy_x, cartesian_type& xy_y, geographic_type& lp_lon, geographic_type& lp_lat) const
                {
                    xy_y /= this->m_proj_parm.C_y;
                    lp_lat = this->m_proj_parm.m ? aasin((this->m_proj_parm.m * xy_y + sin(xy_y)) / this->m_proj_parm.n) :
                        ( this->m_proj_parm.n != 1. ? aasin(sin(xy_y) / this->m_proj_parm.n) : xy_y );
                    lp_lon = xy_x / (this->m_proj_parm.C_x * (this->m_proj_parm.m + cos(xy_y)));
                }
            };

            template <typename Parameters>
            void setup(Parameters& par, par_gn_sinu& proj_parm) 
            {
                boost::ignore_unused_variable_warning(par);
                boost::ignore_unused_variable_warning(proj_parm);
                par.es = 0;
                proj_parm.C_x = (proj_parm.C_y = sqrt((proj_parm.m + 1.) / proj_parm.n))/(proj_parm.m + 1.);
                // par.inv = s_inverse;
                // par.fwd = s_forward;
            }


            // General Sinusoidal Series
            template <typename Parameters>
            void setup_gn_sinu(Parameters& par, par_gn_sinu& proj_parm)
            {
                if (pj_param(par.params, "tn").i && pj_param(par.params, "tm").i) {
                    proj_parm.n = pj_param(par.params, "dn").f;
                    proj_parm.m = pj_param(par.params, "dm").f;
                } else
                    throw proj_exception(-99);
                setup(par, proj_parm);
            }

            // Sinusoidal (Sanson-Flamsteed)
            template <typename Parameters>
            void setup_sinu(Parameters& par, par_gn_sinu& proj_parm)
            {
                    pj_enfn(par.es, proj_parm.en);
            
                if (par.es) {
                // par.inv = e_inverse;
                // par.fwd = e_forward;
                } else {
                    proj_parm.n = 1.;
                    proj_parm.m = 0.;
                    setup(par, proj_parm);
                }
            }

            // Eckert VI
            template <typename Parameters>
            void setup_eck6(Parameters& par, par_gn_sinu& proj_parm)
            {
                proj_parm.m = 1.;
                proj_parm.n = 2.570796326794896619231321691;
                setup(par, proj_parm);
            }

            // McBryde-Thomas Flat-Polar Sinusoidal
            template <typename Parameters>
            void setup_mbtfps(Parameters& par, par_gn_sinu& proj_parm)
            {
                proj_parm.m = 0.5;
                proj_parm.n = 1.785398163397448309615660845;
                setup(par, proj_parm);
            }

        }} // namespace detail::gn_sinu
    #endif // doxygen 

    /*!
        \brief Sinusoidal (Sanson-Flamsteed) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
         - Ellipsoid
        \par Example
        \image html ex_sinu.gif
    */
    template <typename Geographic, typename Cartesian, typename Parameters = parameters>
    struct sinu_ellipsoid : public detail::gn_sinu::base_gn_sinu_ellipsoid<Geographic, Cartesian, Parameters>
    {
        inline sinu_ellipsoid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_ellipsoid<Geographic, Cartesian, Parameters>(par)
        {
            detail::gn_sinu::setup_sinu(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief General Sinusoidal Series projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
         - m= n=
        \par Example
        \image html ex_gn_sinu.gif
    */
    template <typename Geographic, typename Cartesian, typename Parameters = parameters>
    struct gn_sinu_spheroid : public detail::gn_sinu::base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>
    {
        inline gn_sinu_spheroid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>(par)
        {
            detail::gn_sinu::setup_gn_sinu(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Sinusoidal (Sanson-Flamsteed) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
         - Ellipsoid
        \par Example
        \image html ex_sinu.gif
    */
    template <typename Geographic, typename Cartesian, typename Parameters = parameters>
    struct sinu_spheroid : public detail::gn_sinu::base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>
    {
        inline sinu_spheroid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>(par)
        {
            detail::gn_sinu::setup_sinu(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Eckert VI projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_eck6.gif
    */
    template <typename Geographic, typename Cartesian, typename Parameters = parameters>
    struct eck6_spheroid : public detail::gn_sinu::base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>
    {
        inline eck6_spheroid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>(par)
        {
            detail::gn_sinu::setup_eck6(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief McBryde-Thomas Flat-Polar Sinusoidal projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_mbtfps.gif
    */
    template <typename Geographic, typename Cartesian, typename Parameters = parameters>
    struct mbtfps_spheroid : public detail::gn_sinu::base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>
    {
        inline mbtfps_spheroid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_spheroid<Geographic, Cartesian, Parameters>(par)
        {
            detail::gn_sinu::setup_mbtfps(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Factory entry(s)
        template <typename Geographic, typename Cartesian, typename Parameters>
        class gn_sinu_entry : public detail::factory_entry<Geographic, Cartesian, Parameters>
        {
            public :
                virtual projection<Geographic, Cartesian>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<gn_sinu_spheroid<Geographic, Cartesian, Parameters>, Geographic, Cartesian, Parameters>(par);
                }
        };

        template <typename Geographic, typename Cartesian, typename Parameters>
        class sinu_entry : public detail::factory_entry<Geographic, Cartesian, Parameters>
        {
            public :
                virtual projection<Geographic, Cartesian>* create_new(const Parameters& par) const
                {
                    if (par.es)
                        return new base_v_fi<sinu_ellipsoid<Geographic, Cartesian, Parameters>, Geographic, Cartesian, Parameters>(par);
                    else
                        return new base_v_fi<sinu_spheroid<Geographic, Cartesian, Parameters>, Geographic, Cartesian, Parameters>(par);
                }
        };

        template <typename Geographic, typename Cartesian, typename Parameters>
        class eck6_entry : public detail::factory_entry<Geographic, Cartesian, Parameters>
        {
            public :
                virtual projection<Geographic, Cartesian>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<eck6_spheroid<Geographic, Cartesian, Parameters>, Geographic, Cartesian, Parameters>(par);
                }
        };

        template <typename Geographic, typename Cartesian, typename Parameters>
        class mbtfps_entry : public detail::factory_entry<Geographic, Cartesian, Parameters>
        {
            public :
                virtual projection<Geographic, Cartesian>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<mbtfps_spheroid<Geographic, Cartesian, Parameters>, Geographic, Cartesian, Parameters>(par);
                }
        };

        template <typename Geographic, typename Cartesian, typename Parameters>
        inline void gn_sinu_init(detail::base_factory<Geographic, Cartesian, Parameters>& factory)
        {
            factory.add_to_factory("gn_sinu", new gn_sinu_entry<Geographic, Cartesian, Parameters>);
            factory.add_to_factory("sinu", new sinu_entry<Geographic, Cartesian, Parameters>);
            factory.add_to_factory("eck6", new eck6_entry<Geographic, Cartesian, Parameters>);
            factory.add_to_factory("mbtfps", new mbtfps_entry<Geographic, Cartesian, Parameters>);
        }

    } // namespace detail 
    #endif // doxygen

}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_PROJECTIONS_GN_SINU_HPP

