# meta-mailbox
#Add mailbox code:

openbmc$ 

git clone https://github.com/RussellBauer/meta-mailbox.git

nano meta-openbmc-machines/meta-openpower/meta-ibm/meta-stonewither/conf/bblayers.conf.sample

add:

  ##OEROOT##/meta-mailbox \

openbmc$ rm -rf build/conf

(assuming template is still set…)

. openbmc-env

(not sure, I think this is because my recipe is not being picked up, work on later. )

build$ nano conf/local.conf 

add: IMAGE_INSTALL_append = " mailbox"

getting a warning, but it is being built….
WARNING: No bb files matched BBFILE_PATTERN_mailbox '/home/russ/stonewither/openbmc/meta-mailbox/'
