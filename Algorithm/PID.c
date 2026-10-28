/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : PID.c
  * @brief          : PID functions 
  * @author         : GrassFan Wang
  * @date           : 2024/12/29
  * @version        : v1.1
  ******************************************************************************
  * @attention      : To be perfected
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "PID.h"
/* Includes ------------------------------------------------------------------*/

/**
 * @brief Initialize PID parameters.
 * @Param PID: Pointer to PID_Info_TypeDef structure containing PID controller information.
 * @Param Param: Pointer to float array containing PID parameters.
 * @retval PID error status.
 */
static PID_Status_e PID_Param_Init(PID_Info_TypeDef *PID, float Param[PID_PARAMETER_NUM])
{
    // Check if PID type is none or Param is NULL. If so, return PID_FAILED_INIT.
    if (PID->Type == PID_Type_None || Param == NULL)
    {
        return PID_FAILED_INIT;
    }

    // Initialize PID parameters
    PID->Param.KP = Param[0];
    PID->Param.KI = Param[1];
    PID->Param.KD = Param[2];
    PID->Param.Alpha = Param[3];

    if (PID->Param.Alpha > 0.f && PID->Param.Alpha < 1.f) 
    {
        LowPassFilter1p_Init(&PID->Dout_LPF, PID->Param.Alpha);
    }

    PID->Param.Deadband = Param[4];
    PID->Param.LimitIntegral = Param[5];
    PID->Param.LimitOutput = Param[6];

    // Clear PID error counter
    PID->ERRORHandler.ErrorCount = 0;

    // Return PID_ERROR_NONE (no error)
    return PID_ERROR_NONE;
}

/**
 * @brief Clear PID calculation values and reset all outputs to 0.
 * @Param PID: Pointer to PID_Info_TypeDef structure containing PID controller information.
 * @retval None.
 */
static void PID_Calc_Clear(PID_Info_TypeDef *PID)
{
    // Reset all outputs and errors to 0
    memset(PID->Err, 0, sizeof(PID->Err));
    PID->Integral = 0;
    PID->Pout = 0;
    PID->Iout = 0;
    PID->Dout = 0;
    PID->Output = 0;
}

/**
 * @brief Initialize PID controller.
 * @Param PID: Pointer to PID_Info_TypeDef structure containing PID controller information.
 * @Param Type: PID controller type.
 * @Param Param: Pointer to float array containing PID parameters.
 * @retval None.
 */
void PID_Init(PID_Info_TypeDef *PID, PID_Type_e Type, float Param[PID_PARAMETER_NUM])
{
    PID->Type = Type;

    PID->PID_Calc_Clear = PID_Calc_Clear;
    PID->PID_Param_Init = PID_Param_Init;

    PID->PID_Calc_Clear(PID);
    PID->ERRORHandler.Status = PID->PID_Param_Init(PID, Param);
}

/**
  * @brief Check PID error status.
  * @Param PID: Pointer to PID_Info_TypeDef structure containing PID controller information.
  * @retval None.
  */
static void PID_ErrorHandle(PID_Info_TypeDef *PID)
{
    /* Judge NAN/INF */
    if (isnan(PID->Output) == true || isinf(PID->Output) == true)
    {
        PID->ERRORHandler.Status = PID_CALC_NANINF;
    }
}

/**
  * @brief  PID controller calculation function.
  * @Param  *PID pointer to a PID_TypeDef_t structure that contains
  * the configuration information for the specified PID. 
  * @Param  Target  Target for the PID controller.
  * @Param  Measure Measure for the PID controller.
  * @retval the PID Output.
  */
float PID_Calculate(PID_Info_TypeDef *PID, float Target, float Measure)
{
    /* update the PID error status */
    PID_ErrorHandle(PID);
    if (PID->ERRORHandler.Status != PID_ERROR_NONE)
    {
        PID->PID_Calc_Clear(PID);
        return 0;
    }

    /* update the target/measure */
    PID->Target = Target;
    PID->Measure = Measure;

    /* update the error */
    PID->Err[2] = PID->Err[1];
    PID->Err[1] = PID->Err[0];
    PID->Err[0] = PID->Target - PID->Measure;
		
    if (fabsf(PID->Err[0]) >= PID->Param.Deadband)
    {
        /* update the PID controller output */
        if (PID->Type == PID_POSITION)
        {
            /* Update the PID Integral */
            if (PID->Param.KI != 0)
            {
                PID->Integral += PID->Err[0];
            }
            else
            {
                PID->Integral = 0;
            }

            VAL_LIMIT(PID->Integral, -PID->Param.LimitIntegral, PID->Param.LimitIntegral);

            /* Update the Proportional Output, Integral Output, Derivative Output */
            PID->Pout = PID->Param.KP * PID->Err[0];
            PID->Iout = PID->Param.KI * PID->Integral;
            PID->Dout = PID->Param.KD * (PID->Err[0] - PID->Err[1]);

            if (PID->Param.Alpha > 0.f && PID->Param.Alpha < 1.f)
            {
                PID->Dout_LPF.Alpha = PID->Param.Alpha;
                PID->Dout = LowPassFilter1p_Update(&PID->Dout_LPF, PID->Dout);
            }

            /* update the PID output */
            PID->Output = PID->Pout + PID->Iout + PID->Dout;
            VAL_LIMIT(PID->Output, -PID->Param.LimitOutput, PID->Param.LimitOutput);
        }
        else if (PID->Type == PID_VELOCITY)
        {
            /* Update the Proportional Output, Integral Output, Derivative Output */
            PID->Pout = PID->Param.KP * (PID->Err[0] - PID->Err[1]);
            PID->Iout = PID->Param.KI * (PID->Err[0]);
            PID->Dout = PID->Param.KD * (PID->Err[0] - 2.f * PID->Err[1] + PID->Err[2]);

            if (PID->Param.Alpha > 0.f && PID->Param.Alpha < 1.f)
            {
                PID->Dout_LPF.Alpha = PID->Param.Alpha;
                PID->Dout = LowPassFilter1p_Update(&PID->Dout_LPF, PID->Dout);
            }

            /* update the PID output */
            PID->Output += PID->Pout + PID->Iout + PID->Dout;
            VAL_LIMIT(PID->Output, -PID->Param.LimitOutput, PID->Param.LimitOutput);
        }
    }

    return PID->Output;
}

/**
 * @brief Normalize the angle error to [-180, 180] for the shortest path.
 */
float Angle_Error_Normalize(float error)
{
    while (error > 180.0f)  error -= 360.0f;
    while (error < -180.0f) error += 360.0f;
    return error;
}

/**
 * @brief Cascaded PID control for motor angle.
 * @param pos_pid: Pointer to the outer position PID structure.
 * @param vel_pid: Pointer to the inner velocity PID structure.
 * @param target_angle: Desired angle.
 * @param measure_angle: Current angle from encoder.
 * @param measure_vel: Current velocity from encoder/estimator.
 * @retval Final control output (e.g., PWM or Current)
 */
float Cascade_PID_Control(PID_Info_TypeDef *pos_pid, PID_Info_TypeDef *vel_pid,
                          float target_angle, float measure_angle, float measure_vel)
{
    /* 1. Calculate Position Error */
    float angle_error = target_angle - measure_angle;
    
    /* 2. Handle Discontinuity (Shortest Path) */
    angle_error = Angle_Error_Normalize(angle_error);
    
    /* 3. Outer Loop: Position PID */
    // Temporarily update error to use the standard calculation function
    pos_pid->Target = angle_error;
    pos_pid->Measure = 0; // Error is already calculated
    
    // We manually trigger the calculation or use a modified PID_Calculate
    float target_velocity = PID_Calculate(pos_pid, angle_error, 0);

    /* 4. Inner Loop: Velocity PID */
    // The output of position PID is the target for velocity PID
    float final_output = PID_Calculate(vel_pid, target_velocity, measure_vel);

    return final_output;
}

/**
 * @brief Single loop PID control for angle with shortest path logic.
 * @param pid: Pointer to the PID structure (PID_POSITION type recommended).
 * @param target_angle: Desired angle.
 * @param measure_angle: Current angle.
 * @retval Final control output.
 */
float Single_Angle_PID_Calculate(PID_Info_TypeDef *pid, float target_angle, float measure_angle)
{
    /* 1. Calculate the shortest path error */
    float angle_error = target_angle - measure_angle;
    angle_error = Angle_Error_Normalize(angle_error);
    
    /* 2. Execute PID calculation */
    /* Note: To force the PID to use our normalized error, 
       we bypass the internal (Target - Measure) calculation 
       by setting Measure to 0 and Target to the error. */
    return PID_Calculate(pid, angle_error, 0.0f);
}
