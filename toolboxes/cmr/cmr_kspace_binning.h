/** \file   cmr_kspace_binning.h
    \brief  Implement kspace binning recon for 2D acquisition
            The input has dimension [RO E1 CHA N S SLC]
            Temporal dimension is N and S is motion sharing dimension, e.g. contrast or set
    \author Hui Xue
*/

#pragma once

#include "cmr_export.h"

#include "GadgetronTimer.h"

#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"
#include "ismrmrd/meta.h"

#include "mri_core_def.h"
#include "mri_core_data.h"
#include "mri_core_utility.h"

#include "ImageIOAnalyze.h"

namespace Gadgetron { 

    template <typename T>
    class EXPORTCMR KSpaceBinningObj
    {
    public:

        KSpaceBinningObj();
        virtual ~KSpaceBinningObj();

        // ------------------------------------
        /// recon outputs
        // ------------------------------------
        /// binning reconstructed images, headers and meta attributes
        /// [RO E1 1 output_N_ S]
        hoNDArray< std::complex<T> > complex_image_binning_;

        /// raw reconstructed images, headers and meta attributes
        /// [RO E1 dstCHA N S]
        hoNDArray< std::complex<T> > full_kspace_raw_;
        /// complex images
        hoNDArray< std::complex<T> > complex_image_raw_;
        /// coil map, [RO E1 dstCHA]
        hoNDArray< std::complex<T> > coil_map_raw_;

        /// mean RR interval in ms
        float mean_RR_;

        // ------------------------------------
        /// recon inputs
        // ------------------------------------
        /// number of required binning output along N
        size_t output_N_;

        /// acceleration factor
        float accel_factor_E1_;

        /// whether random sampling is used
        bool random_sampling_;

        /// input kspace data and headers
        /// data: [RO E1 CHA N S]
        /// only 2D binning is supported, so E2==1
        /// N is the temporal dimension
        /// S is the motion sharing dimension, e.g. contrast or set
        /// incoming image should already be in eigeh channel, with the high energy channel first
        hoNDArray< std::complex<T> > data_;
        SamplingDescription sampling_;

        /// [E1 N S]
        hoNDArray< ISMRMRD::AcquisitionHeader > headers_;

        // ------------------------------------
        /// buffer for recon
        // ------------------------------------
        /// time stamp in ms for kspace lines [E1 N S]
        hoNDArray<float> time_stamp_;
        /// cardiac phase time ratio (CPT) [0 1] for kspace lines, [E1 N S]
        hoNDArray<float> cpt_time_stamp_;
        /// time stamp for every raw images, [N S]
        hoNDArray<float> phs_time_stamp_;
        /// cardiac phase time ratio for every raw images, [N S]
        hoNDArray<float> phs_cpt_time_stamp_;

        // buffer to store index of heart beat for every kspace line; the first heart beat has index 0
        hoNDArray<size_t> ind_heart_beat_;
        // arrays to store the starting and ending [E1 N] for every heart beat
        // for every S, one std::vector is created to store starting/ending E1 and PHS of heart beats
        std::vector< std::vector< std::pair<long long, long long> > > starting_heart_beat_;
        std::vector< std::vector< std::pair<long long, long long> > > ending_heart_beat_;

        // output cardiac phases
        std::vector<float> desired_cardiac_phases_;
        // output cardiac phase ratio
        std::vector<float> desired_cpt_;

        /// respiratory navigator signal for every kspace line, [E1 N S]
        hoNDArray<float> navigator_;
    };

    template<typename T> 
    class CmrKSpaceBinning
    {
    public:

        typedef CmrKSpaceBinning<T> Self;
        typedef hoNDArray< std::complex<T> > ArrayType;

        CmrKSpaceBinning();
        virtual ~CmrKSpaceBinning();

        // ======================================================================================
        // perform the binning reconstruction
        // ======================================================================================
        virtual void process_binning_recon();

        // ------------------------------------
        /// binning object, storing the kspace data and results
        // ------------------------------------
        KSpaceBinningObj<T> binning_obj_;

        // ======================================================================================
        /// parameter for overall workflow
        // ======================================================================================

        // whether true, raw data recon step will produce multiple channels
        bool use_multiple_channel_recon_;

        // if true, and use_multiple_channel_recon_==true, then
        // the binning recon step will perform parallel imaging
        bool use_paralell_imaging_binning_recon_;

        // if true, and use_multiple_channel_recon_==ture and use_paralell_imaging_binning_recon_==true
        // the binning recon step will perform nonlinear reconstruction
        bool use_nonlinear_binning_recon_;

        // if true, respiratory navigator will be estimated
        // if false, user can supply external navigator by filling in binning_obj_.navigator_
        bool estimate_respiratory_navigator_;

        // ======================================================================================
        /// parameter for respiratory navigator estimation
        // ======================================================================================

        // ======================================================================================
        /// parameter for raw image reconstruction
        // ======================================================================================

        // time tick for every time stamp unit, in ms (e.g. 2.5ms)
        float time_tick_;

        // if a heart beat RR is not in the range of [ (1-arrhythmiaRejectorFactor)*meanRR (1+arrhythmiaRejectorFactor)*meanRR], it will be rejected
        float arrhythmia_rejector_factor_;

        // ----------------------
        // grappa raw data reconstruction
        // ----------------------
        int grappa_kSize_RO_;
        int grappa_kSize_E1_;
        double grappa_reg_lamda_;

        // down stream coil compression
        // if downstream_coil_compression_num_modesKept > 0, this number of channels will be used as the dst channels
        // if downstream_coil_compression_num_modesKept==0 and downstream_coil_compression_thres>0, 
        // the number of dst channels will be determined  by this threshold
        size_t downstream_coil_compression_num_modesKept_;
        double downstream_coil_compression_thres_;

        // ----------------------
        // spirit raw data reconstruction, if random sampling was used
        // ----------------------
        // to be implemented

        // ======================================================================================
        /// parameter for cardiac binning
        // ======================================================================================

        // ======================================================================================
        /// parameter for recon after binning
        // ======================================================================================

        // ======================================================================================
        /// parameter for debugging
        // ======================================================================================
        bool verbose_;
        std::string debug_folder_;
        bool perform_timing_;

        // clock for timing
        Gadgetron::GadgetronTimer gt_timer_local_;
        Gadgetron::GadgetronTimer gt_timer_;

        // exporter
        Gadgetron::ImageIOAnalyze gt_exporter_;

    protected:

        // ======================================================================================
        // perform every steps
        // ======================================================================================

        // perform raw data recon on binning_obj_.data_
        // fill the binning_obj_.full_kspace_raw_, binning_obj_.complex_image_raw_, binning_obj_.coil_map_raw_
        virtual void perform_raw_data_recon();

        /// this function parses all recorded time stamps and computes cpt time stamps
        /// first, all time stamps are converted into the unit of ms
        /// second, a linear fit is performed to reduce then quantization error
        /// third, for all missing lines, its time stamps are estimated
        /// fourth, the cpt time stamps are analyzed to find out every heart beats
        /// fifth, the mean RR and desired cardiac phases are computed
        virtual void estimate_time_stamps();

        /// estimate the respiratory navigator from the images
        virtual void estimate_respiratory_navigator();

        /// find the best heart beat
        /// the strategy is to find the heart beat with minimal intra-heart-beat navigator signal variance
        /// bestHB for every S
        virtual void find_best_heart_beat(std::vector<size_t>& bestHB);

        /// reject heart beat with irregular rhythm
        virtual void reject_irregular_heart_beat();
    };
}