# docker image for docker-ptf-centec

DOCKER_PTF_CENTEC = docker-ptf-centec.gz
$(DOCKER_PTF_CENTEC)_PATH = $(DOCKERS_PATH)/docker-ptf-saithrift
$(DOCKER_PTF_CENTEC)_DEPENDS += $(PYTHON_SAITHRIFT)
$(DOCKER_PTF_CENTEC)_LOAD_DOCKERS += $(DOCKER_PTF)
SONIC_DOCKER_IMAGES += $(DOCKER_PTF_CENTEC)
