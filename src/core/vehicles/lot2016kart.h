#ifndef __LOT2016KART_H__
#define __LOT2016KART_H__

#include "src/core/tire/tire_pacejka.h"
#include "src/core/chassis/axle_car_6dof.h"
#include "src/core/chassis/chassis_car_6dof.h"
#include "src/core/vehicles/track_by_polynomial.h"
#include "src/core/vehicles/track_by_arcs.h"
#include "src/core/vehicles/road_cartesian.h"
#include "src/core/vehicles/road_curvilinear.h"
#include "src/core/vehicles/dynamic_model_car.h"
#include "lion/thirdparty/include/cppad/cppad.hpp"

template<typename Timeseries_t>
class lot2016kart
{
 public:
    lot2016kart() = delete;

    using Front_left_tire_type  = Tire_pacejka_std<Timeseries_t,0,0>;
    using Front_right_tire_type = Tire_pacejka_std<Timeseries_t,Front_left_tire_type::STATE_END,Front_left_tire_type::CONTROL_END>;
    using Rear_left_tire_type   = Tire_pacejka_std<Timeseries_t,Front_right_tire_type::STATE_END,Front_right_tire_type::CONTROL_END>;
    using Rear_right_tire_type  = Tire_pacejka_std<Timeseries_t,Rear_left_tire_type::STATE_END,Rear_left_tire_type::CONTROL_END>;

    using Front_axle_t          = Axle_car_6dof<Timeseries_t,Front_left_tire_type,Front_right_tire_type,STEERING_FREE_ROLL,Rear_right_tire_type::STATE_END,Rear_right_tire_type::CONTROL_END>;
    using Rear_axle_t           = Axle_car_6dof<Timeseries_t,Rear_left_tire_type,Rear_right_tire_type,POWERED_WITHOUT_DIFFERENTIAL,Front_axle_t::STATE_END,Front_axle_t::CONTROL_END>;
    using Chassis_t             = Chassis_car_6dof<Timeseries_t,Front_axle_t,Rear_axle_t,Rear_axle_t::STATE_END,Rear_axle_t::CONTROL_END>;

    using Road_cartesian_t   = Road_cartesian<Timeseries_t,Chassis_t::STATE_END,Chassis_t::CONTROL_END>;

    template<typename Track_t>
    using Road_curvilinear_t = Road_curvilinear<Timeseries_t,Track_t,Chassis_t::STATE_END,Chassis_t::CONTROL_END>;
 
 private:
    
    template<typename Road_type>
    class Dynamic_model : public Dynamic_model_car<Timeseries_t,Chassis_t,Road_type,Road_type::STATE_END,Road_type::CONTROL_END>
    {
     public:
        using Road_t          = Road_type;
        using Dynamic_model_t = Dynamic_model_car<Timeseries_t,Chassis_t, Road_type, Road_type::STATE_END, Road_type::CONTROL_END>;

        Dynamic_model() {}
        Dynamic_model(Xml_document& database, const Road_t& road = Road_t()) : Dynamic_model_t(database,road) {}

        // Steady-state computation
        static constexpr const size_t N_SS_VARS = 6;
        static constexpr const size_t N_SS_EQNS = 12;

        static std::pair<std::vector<scalar>,std::vector<scalar>> steady_state_variable_bounds() 
        {
            return { { -0.5, 1.0e-4, -10.0*DEG, -10.0*DEG, -10.0*DEG, -10.0*DEG },
                     {  0.5, 0.04  ,  10.0*DEG,  10.0*DEG,  10.0*DEG,  10.0*DEG } };
        }


        static std::pair<std::vector<scalar>,std::vector<scalar>> steady_state_constraint_bounds() 
        {
            return { {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -0.11, -0.11, -0.09, -0.09, -0.09, -0.09},
                     {0.0, 0.0, 0.0, 0.0, 0.0, 0.0,  0.11,  0.11,  0.09,  0.09,  0.09,  0.09} };
        }

        std::tuple<std::array<Timeseries_t,N_SS_EQNS>,
                   std::array<Timeseries_t,Dynamic_model_t::NSTATE>,
                   std::array<Timeseries_t,Dynamic_model_t::NCONTROL>>
            steady_state_equations(const std::array<Timeseries_t,N_SS_VARS>& x, 
                                   const Timeseries_t& ax, 
                                   const Timeseries_t& ay, 
                                   const Timeseries_t& v)
        {
            // The content of x is: x = [w_axle, z, phi, mu, psi, delta]
            // Construct state and controls
            const Timeseries_t& psi = x[4];
            const Timeseries_t omega = ay/v;
             
            // Construct the state
            std::array<Timeseries_t,Dynamic_model_t::NSTATE> q;
            q[Dynamic_model_t::Chassis_type::rear_axle_type::IOMEGA_AXLE] = (x[0]+1.0)*v/0.139;
            q[Dynamic_model_t::Chassis_type::IU]                          = v*cos(psi);
            q[Dynamic_model_t::Chassis_type::IV]                          = -v*sin(psi);
            q[Dynamic_model_t::Chassis_type::IOMEGA]                      = omega;
            q[Dynamic_model_t::Chassis_type::IZ]                          = x[1];
            q[Dynamic_model_t::Chassis_type::IPHI]                        = x[2];
            q[Dynamic_model_t::Chassis_type::IMU]                         = x[3];
            q[Dynamic_model_t::Chassis_type::IDZ]                         = 0.0;
            q[Dynamic_model_t::Chassis_type::IDPHI]                       = 0.0;
            q[Dynamic_model_t::Chassis_type::IDMU]                        = 0.0;
            q[Dynamic_model_t::Road_type::IX]                             = 0.0;
            q[Dynamic_model_t::Road_type::IY]                             = 0.0;
            q[Dynamic_model_t::Road_type::IPSI]                           = x[4];
        
            // Construct the controls
            std::array<Timeseries_t,Dynamic_model_t::NCONTROL> u;
            u[Dynamic_model_t::Chassis_type::front_axle_type::ISTEERING] = x[5];
            u[Dynamic_model_t::Chassis_type::rear_axle_type::ITORQUE]    = 0.0;
        
            // Compute time derivative
            auto dqdt = (*this)(q,u,0.0);
        
            // Compute constraints
            std::array<Timeseries_t,N_SS_EQNS> constraints;
        
            constraints[0] = dqdt[Dynamic_model_t::Chassis_type::IID2Z];
            constraints[1] = dqdt[Dynamic_model_t::Chassis_type::IIDOMEGA];
            constraints[2] = dqdt[Dynamic_model_t::Chassis_type::IID2PHI];
            constraints[3] = dqdt[Dynamic_model_t::Chassis_type::IID2MU];
            constraints[4] = dqdt[Dynamic_model_t::Chassis_type::IIDU]*sin(psi)
                            + dqdt[Dynamic_model_t::Chassis_type::IIDV]*cos(psi);
            constraints[5] = ax - dqdt[Dynamic_model_t::Chassis_type::IIDU]*cos(psi)
                                 + dqdt[Dynamic_model_t::Chassis_type::IIDV]*sin(psi);
         
        
            constraints[6] = this->get_chassis().get_rear_axle().template get_tire<0>().get_kappa();
            constraints[7] = this->get_chassis().get_rear_axle().template get_tire<1>().get_kappa();
            constraints[8] = this->get_chassis().get_rear_axle().template get_tire<0>().get_lambda();
            constraints[9] = this->get_chassis().get_rear_axle().template get_tire<1>().get_lambda();
            constraints[10] = this->get_chassis().get_front_axle().template get_tire<0>().get_lambda();
            constraints[11] = this->get_chassis().get_front_axle().template get_tire<1>().get_lambda();
        
            return {constraints,q,u};
        }
    };

 public:
    
    using cartesian = Dynamic_model<Road_cartesian_t>;

    template<typename Track_t>
    using curvilinear = Dynamic_model<Road_curvilinear_t<Track_t>>;

    using curvilinear_p = curvilinear<Track_by_polynomial>;
    using curvilinear_a = curvilinear<Track_by_arcs>;
};


struct lot2016kart_all
{
    lot2016kart_all(Xml_document& database_xml)
    : cartesian_scalar(database_xml), 
      curvilinear_scalar(database_xml),
      cartesian_ad(database_xml),
      curvilinear_ad(database_xml) 
    {}

    lot2016kart<scalar>::cartesian                 cartesian_scalar;
    lot2016kart<scalar>::curvilinear_a             curvilinear_scalar;
    lot2016kart<CppAD::AD<scalar>>::cartesian      cartesian_ad;
    lot2016kart<CppAD::AD<scalar>>::curvilinear_a  curvilinear_ad;
};


#endif
