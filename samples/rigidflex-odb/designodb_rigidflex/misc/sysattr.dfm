UNITS=INCH


TEXT {
    NAME=.layer_class
    PROMPT=Layer Class
    MIN_LEN=0
    MAX_LEN=1000
    ENTITY=layer
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_route_keepin_lyr
    PROMPT=Route Keep In Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_comp_keepin_lyr
    PROMPT=Component Keep In Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_tp_keepin_lyr
    PROMPT=Test Point Keep In Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_route_keepout_lyr
    PROMPT=Route Keep Out Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_comp_keepout_lyr
    PROMPT=Component Keep Out Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_pad_keepout_lyr
    PROMPT=Pad Keep Out Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_via_keepout_lyr
    PROMPT=Via Keep Out Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_trace_keepout_lyr
    PROMPT=Trace Keep Out Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_plane_keepout_lyr
    PROMPT=Plane Keep Out Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_comp_height_lyr
    PROMPT=Comp. Height Restriction Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.drc_tp_keepout_lyr
    PROMPT=Test Point Keep Out Layer
    MIN_LEN=0
    MAX_LEN=64
    ENTITY=job
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.fab_drc
    PROMPT=Fab DRC
    MIN_LEN=0
    MAX_LEN=20
    ENTITY=step
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}

TEXT {
    NAME=.hdi_drc
    PROMPT=HDI DRC
    MIN_LEN=0
    MAX_LEN=20
    ENTITY=step
    DEF=
    GROUP=Custom
    OPTIONS=
    DEF_OPT=
}


OPTION {
    NAME=.comp_mount_type
    PROMPT=Mount Type
    OPTIONS=other;smt;thmt;pressfit
    DELETED=NO;NO;NO;NO
    ENTITY=package;component
    DEF=other
    GROUP=Component
}

OPTION {
    NAME=.comp_type2
    PROMPT=Type 2
    OPTIONS=axial;axial-large;bga;cbga;cob;csp;dip;dip300;dip600;discrete;discrete201;discrete402;discrete603;electro-mech;flipchip;label;lcc;lqfp;pfconn;pga;pihconn-inline;pihconn-rt-angle;pihmisc;pih-polar;plcc;pqfp;printed;qfp;radial;radial-tall;sip;smtconn;smtelect-mech;smtmisc;smtmixedconn;smtpolar;socket;soic;soj;solderable-mech;sop-ssop;sot;tab;tqfp;tsoic;tsop;tsop-tssop;reserved47;reserved48;reserved49;reserved50;reserved51;reserved52;reserved53;reserved54;reserved55;reserved56;reserved57;reserved58;reserved59;reserved60;reserved61;reserved62;reserved63;reserved64;reserved65;reserved66;reserved67;reserved68;reserved69;reserved70;reserved71;reserved72;reserved73;reserved74;reserved75;reserved76;reserved77;reserved78;reserved79;reserved80;reserved81;reserved82;reserved83;reserved84;reserved85;reserved86;reserved87;reserved88;reserved89;reserved90;reserved91;reserved92;reserved93;reserved94;reserved95;reserved96;reserved97;reserved98;reserved99;reserved100
    DELETED=NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO;NO
    ENTITY=package;component
    DEF=axial
    GROUP=DFx Component
}



