/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : PID.c
  * @brief          : PID functions 
  * @author         : Yan Yuanbin
  * @date           : 2023/04/27
  * @version        : v1.0
  ******************************************************************************
  * @attention      : To be perfected
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CONTROLLER_PID_H
#define CONTROLLER_PID_H

/* Includes ------------------------------------------------------------------*/
#include "robot_config.h"
#include "LPF.h"
/* Exported defines -----------------------------------------------------------*/
/**
 * @brief macro definition of the VAL_LIMIT that restricts the value of the specified variable.
 * @param x: the specified variable
 * @param min: the minimum value of the specified variable
 * @param max: the maximum value of the specified variable
 * @retval none
 */
#define VAL_LIMIT(x,min,max)  do{ \
                                    if ((x) > (max)) {(x) = (max);} \
                                    else if ((x) < (min)) {(x) = (min);} \
                                }while(0U)

/**
 * @brief macro definition of the number of pid parameters
*/
#ifndef PID_PARAMETER_NUM
#define PID_PARAMETER_NUM 7							
#endif

/* Exported types ------------------------------------------------------------*/
/**
 * @brief typedef enum that contains the Error status for the pid controller.
 */
typedef enum
{
    PID_ERROR_NONE = 0x00U,        /*!< No error */
    PID_FAILED_INIT = 0x01U,        /*!< Initialization failed */
	PID_CALC_NANINF = 0x02U,      /*!< Not-a-number (NaN) or infinity is generated */
    PID_Status_NUM,
}PID_Status_e;

/**
 * @brief typedef enum that contains the type for the pid controller.
 */
typedef enum
{
		PID_Type_None = 0x00U,         /*!< No Type */
		PID_POSITION = 0x01U,          /*!< position pid */
		PID_VELOCITY = 0x02U,          /*!< velocity pid */
    PID_TYPE_NUM,
}PID_Type_e;

/**
 * @brief typedef structure that contains the information for the pid Error handler.
 */
typedef struct
{
    uint16_t ErrorCount;    /*!< Error status judgment count */
    PID_Status_e Status;    /*!< Error Status */
}PID_ErrorHandler_Typedef;

/**
 * @brief typedef structure that contains the parameters for the pid controller.
 */
typedef struct
{
    float KP;                      // Proportional gain
    float KI;                      // Integral gain
    float KD;                      // Derivative gain
    float Alpha;                   // First-order low-pass filter coefficient for derivative
    float Deadband;                // Deadband: PID calculation stops if the absolute error is smaller than this value
    float LimitIntegral;           // Integral output limit
    float LimitOutput;             // Total output limit
} PID_Parameter_Typedef;

/**
 * @brief typedef structure that contains the information for the pid controller.
 */
typedef struct _PID_TypeDef
{
    PID_Type_e Type;               // PID type: Position or Velocity

    float Target;                  // Target value
    float Measure;                 // Measured value

    float Err[3];                  // Error history: [0] current, [1] last, [2] last last
    float Integral;                // Accumulated integral value
    float Pout;                    // Proportional output: KP * error
    float Iout;                    // Integral output: KI * integral
    float Dout;                    // Derivative output: KD * derivative (filtered)
    float Output;                  // Total output: Pout + Iout + Dout

    LowPassFilter1p_Info_TypeDef Dout_LPF; // First-order low-pass filter for derivative component

    PID_Parameter_Typedef Param;            // PID parameter structure
    PID_ErrorHandler_Typedef ERRORHandler;  // PID error handler structure

    /**
     * @brief Function pointer to initialize PID parameters.
     * @param PID: Pointer to PID_Info_TypeDef structure.
     * @param Param: Pointer to float array containing parameter values.
     * @retval PID error status.
     */
    PID_Status_e (*PID_Param_Init)(struct _PID_TypeDef *PID, float *Param);

    /**
     * @brief Function pointer to clear PID calculation state.
     * @param PID: Pointer to PID_Info_TypeDef structure.
     * @retval None.
     */
    void (*PID_Calc_Clear)(struct _PID_TypeDef *PID);
                
} PID_Info_TypeDef;

/* Exported functions prototypes ---------------------------------------------*/
/**
 * @brief Initializes the PID Controller.
 */

extern void PID_Init(PID_Info_TypeDef *Pid,PID_Type_e type,float para[PID_PARAMETER_NUM]);

/**
  * @brief  Caculate the PID Controller
  */
extern float PID_Calculate(PID_Info_TypeDef *PID, float Target,float Measure);

/**
  * @brief  Caculate the Cascade PID Controller
  */
float Cascade_PID_Control(PID_Info_TypeDef *pos_pid, PID_Info_TypeDef *vel_pid,
                          float target_angle, float measure_angle, float measure_vel);

float Single_Angle_PID_Calculate(PID_Info_TypeDef *PID, float Target, float Measure);

/**
 * @brief Normalize the angle error to [-180, 180] for the shortest path.
 */
float Angle_Error_Normalize(float error);

#endif //CONTROLLER_PID_H

