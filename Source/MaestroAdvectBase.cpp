#include <Maestro.H>
#include <Maestro_F.H>

using namespace amrex;

void 
Maestro::AdvectBaseDens(BaseState<Real>& rho0_predicted_edge)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::AdvectBaseDens()", AdvectBaseDens); 

    if (!spherical) {
        AdvectBaseDensPlanar(rho0_predicted_edge);
        RestrictBase(rho0_new, true);
        FillGhostBase(rho0_new, true);
    } else {
        AdvectBaseDensSphr(rho0_predicted_edge);
    }
}

void 
Maestro::AdvectBaseDensPlanar(BaseState<Real>& rho0_predicted_edge)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::AdvectBaseDensPlanar()", AdvectBaseDensPlanar); 

    BaseState<Real> force(max_radial_level+1,nr_fine);

    // zero the new density so we don't leave a non-zero density in fine radial
    // regions that no longer have a corresponding full state
    rho0_new.setVal(0.0);

    // Predict rho_0 to vertical edges

    for (int n = 0; n <= max_radial_level; ++n) {

        const auto& rho0_old_p = rho0_old; 
        const auto& w0_p = w0;  

        const Real dr_lev = dr(n);

        for (int i = 1; i <= numdisjointchunks(n); ++i) {

            int lo = r_start_coord(n,i);
            int hi = r_end_coord(n,i);

            AMREX_PARALLEL_FOR_1D(hi-lo+1, j, {
                int r = j + lo;

                force(n,r) = -rho0_old_p(n,r) * (w0_p(n,r+1) - w0_p(n,r)) / dr_lev;
            });
        }
    }

    MakeEdgeState1d(rho0_old, rho0_predicted_edge, force);

    for (int n = 0; n <= max_radial_level; ++n) {

        const auto& rho0_old_p = rho0_old; 
        auto& rho0_new_p = rho0_new; 
        const auto& w0_p = w0;  

        const Real dr_lev = dr(n);
        const Real dt_loc = dt;
        
        for (int i = 1; i <= numdisjointchunks(n); ++i) {

            int lo = r_start_coord(n,i);
            int hi = r_end_coord(n,i);

            AMREX_PARALLEL_FOR_1D(hi-lo+1, j, {
                int r = j + lo;

                rho0_new_p(n,r) = rho0_old_p(n,r)
                    - dt_loc/dr_lev * (rho0_predicted_edge(n,r+1)*w0_p(n,r+1) - rho0_predicted_edge(n,r)*w0_p(n,r));
            });
        }
    }
}

void 
Maestro::AdvectBaseDensSphr(BaseState<Real>& rho0_predicted_edge)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::AdvectBaseDensSphr()", AdvectBaseDensSphr);

    const Real dr0 = dr(0);
    const Real dtdr = dt / dr0;
    BaseState<Real> force(nr_fine);

    // Predict rho_0 to vertical edges
    const auto& rho0_old_p = rho0_old; 
    auto& rho0_new_p = rho0_new;
    const auto& w0_p = w0; 
    const auto& r_cc_loc_p = r_cc_loc;
    const auto& r_edge_loc_p = r_edge_loc;

    AMREX_PARALLEL_FOR_1D(nr_fine, r, {
        force(r) = -rho0_old_p(0,r) * (w0_p(0,r+1) - w0_p(0,r)) / dr0 - 
            rho0_old_p(0,r)*(w0_p(0,r) + w0_p(0,r+1))/r_cc_loc_p(0,r);
    });

    MakeEdgeState1d(rho0_old, rho0_predicted_edge, force);

    AMREX_PARALLEL_FOR_1D(nr_fine, r, {
        rho0_new_p(0,r) = rho0_old_p(0,r) - dtdr/(r_cc_loc_p(0,r)*r_cc_loc_p(0,r)) * 
            (r_edge_loc_p(0,r+1)*r_edge_loc_p(0,r+1) * rho0_predicted_edge(0,r+1) * w0_p(0,r+1) - 
            r_edge_loc_p(0,r)*r_edge_loc_p(0,r) * rho0_predicted_edge(0,r) * w0_p(0,r));
    });
}

void 
Maestro::AdvectBaseEnthalpy(BaseState<Real>& rhoh0_predicted_edge)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::AdvectBaseEnthalpy()", AdvectBaseEnthalpy); 

    if (!spherical) {
        AdvectBaseEnthalpyPlanar(rhoh0_predicted_edge);
        RestrictBase(rhoh0_new, true);
        FillGhostBase(rhoh0_new, true);
    } else {
        AdvectBaseEnthalpySphr(rhoh0_predicted_edge);
    }
}

void 
Maestro::AdvectBaseEnthalpyPlanar(BaseState<Real>& rhoh0_predicted_edge)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::AdvectBaseEnthalpyPlanar()", AdvectBaseEnthalpyPlanar); 

    BaseState<Real> force(max_radial_level+1, nr_fine);

    // zero the new enthalpy so we don't leave a non-zero enthalpy in fine radial
    // regions that no longer have a corresponding full state
    rhoh0_new.setVal(0.0);

    // Update (rho h)_0

    for (int n = 0; n <= max_radial_level; ++n) {

        const auto& rhoh0_old_p = rhoh0_old; 
        const auto& w0_p = w0;  
        const auto& psi_p = psi;  

        const Real dr_lev = dr(n);

        for (int i = 1; i <= numdisjointchunks(n); ++i) {

            int lo = r_start_coord(n,i);
            int hi = r_end_coord(n,i);

            // here we predict (rho h)_0 on the edges
            AMREX_PARALLEL_FOR_1D(hi-lo+1, j, {
                int r = j + lo;
                force(n,r) = -rhoh0_old_p(n,r) * (w0_p(n,r+1) - w0_p(n,r)) / dr_lev 
                    + psi_p(n,r);
            });
        }
    }

    MakeEdgeState1d(rhoh0_old, rhoh0_predicted_edge, force);

    for (int n = 0; n <= max_radial_level; ++n) {

        const auto& rhoh0_old_p = rhoh0_old; 
        auto& rhoh0_new_p = rhoh0_new; 
        const auto& w0_p = w0;  
        const auto& psi_p = psi; 

        const Real dr_lev = dr(n);
        const Real dt_loc = dt;

        for (int i = 1; i <= numdisjointchunks(n); ++i) {

            int lo = r_start_coord(n,i);
            int hi = r_end_coord(n,i);

            // update (rho h)_0
            AMREX_PARALLEL_FOR_1D(hi-lo+1, j, {
                int r = j + lo;
                rhoh0_new_p(n,r) = rhoh0_old_p(n,r) 
                    - dt_loc/dr_lev * (rhoh0_predicted_edge(n,r+1)*w0_p(n,r+1) - rhoh0_predicted_edge(n,r)*w0_p(n,r)) + dt_loc * psi_p(n,r);
            });
        }
    }
}

void 
Maestro::AdvectBaseEnthalpySphr(BaseState<Real>& rhoh0_predicted_edge)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::AdvectBaseEnthalpySphr()", AdvectBaseEnthalpySphr);

    const Real dr0 = dr(0);
    const Real dtdr = dt / dr0;
    const Real dt_loc = dt;

    BaseState<Real> force(nr_fine);

    // predict (rho h)_0 on the edges
    const auto& rhoh0_old_p = rhoh0_old; 
    auto& rhoh0_new_p = rhoh0_new;
    const auto& w0_p = w0; 
    const auto& r_cc_loc_p = r_cc_loc;
    const auto& r_edge_loc_p = r_edge_loc;
    const auto& psi_p = psi; 

    AMREX_PARALLEL_FOR_1D(nr_fine, r, {
        force(r) = -rhoh0_old_p(0,r) * (w0_p(0,r+1) - w0_p(0,r)) / dr0 - 
            rhoh0_old_p(0,r)*(w0_p(0,r) + w0_p(0,r+1))/r_cc_loc_p(0,r) + psi_p(0,r);
    });

    MakeEdgeState1d(rhoh0_old, rhoh0_predicted_edge, force);

    AMREX_PARALLEL_FOR_1D(nr_fine, r, {

        rhoh0_new_p(0,r) = rhoh0_old_p(0,r) - dtdr/(r_cc_loc_p(0,r)*r_cc_loc_p(0,r)) * 
            (r_edge_loc_p(0,r+1)*r_edge_loc_p(0,r+1) * rhoh0_predicted_edge(0,r+1) * w0_p(0,r+1) - 
            r_edge_loc_p(0,r)*r_edge_loc_p(0,r) * rhoh0_predicted_edge(0,r) * w0_p(0,r)) + 
            dt_loc * psi_p(0,r);
    });
}
