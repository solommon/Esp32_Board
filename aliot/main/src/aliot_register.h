#ifndef _ALIOT_REGISTER_H_
#define _ALIOT_REGISTER_H_

/**
 * 发起动态注册，获取设备秘钥
 * @param 无
 * @return 无
 */
void aliot_start_register(void);

/**
 * 查询注册结果
 * @param 无
 * @return 成功非0,失败0
 */
char aliot_register_result(void);

#endif
