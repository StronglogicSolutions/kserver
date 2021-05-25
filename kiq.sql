--
-- PostgreSQL database dump
--

-- Dumped from database version 12.6 (Ubuntu 12.6-0ubuntu0.20.10.1)
-- Dumped by pg_dump version 12.6 (Ubuntu 12.6-0ubuntu0.20.10.1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

--
-- Name: get_recurring_seconds(integer); Type: FUNCTION; Schema: public; Owner: kiqadmin
--

CREATE FUNCTION public.get_recurring_seconds(n integer) RETURNS integer
    LANGUAGE plpgsql
    AS $$
BEGIN
CASE n
WHEN 0 THEN
RETURN 0;
WHEN 1 THEN
RETURN 3600;
WHEN 2 THEN
RETURN 86400;
WHEN 3 THEN
RETURN 2419200;
WHEN 4 THEN
RETURN 31536000;
END CASE;
END;
$$;


ALTER FUNCTION public.get_recurring_seconds(n integer) OWNER TO kiqadmin;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: apps; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.apps (
    id integer NOT NULL,
    path text,
    data text,
    mask integer,
    name text,
    internal boolean DEFAULT false
);


ALTER TABLE public.apps OWNER TO kiqadmin;

--
-- Name: apps_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.apps_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.apps_id_seq OWNER TO kiqadmin;

--
-- Name: apps_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.apps_id_seq OWNED BY public.apps.id;


--
-- Name: file; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.file (
    id integer NOT NULL,
    name text,
    sid integer
);


ALTER TABLE public.file OWNER TO kiqadmin;

--
-- Name: file_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.file_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.file_id_seq OWNER TO kiqadmin;

--
-- Name: file_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.file_id_seq OWNED BY public.file.id;


--
-- Name: platform; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.platform (
    id integer NOT NULL,
    name text,
    method text DEFAULT 'process'::text NOT NULL
);


ALTER TABLE public.platform OWNER TO kiqadmin;

--
-- Name: platform_affiliate_user; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.platform_affiliate_user (
    id integer NOT NULL,
    uid integer NOT NULL,
    a_uid integer NOT NULL
);


ALTER TABLE public.platform_affiliate_user OWNER TO kiqadmin;

--
-- Name: platform_affiliate_user_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.platform_affiliate_user_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.platform_affiliate_user_id_seq OWNER TO kiqadmin;

--
-- Name: platform_affiliate_user_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.platform_affiliate_user_id_seq OWNED BY public.platform_affiliate_user.id;


--
-- Name: platform_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.platform_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.platform_id_seq OWNER TO kiqadmin;

--
-- Name: platform_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.platform_id_seq OWNED BY public.platform.id;


--
-- Name: platform_post; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.platform_post (
    id integer NOT NULL,
    pid integer NOT NULL,
    o_pid integer,
    unique_id text NOT NULL,
    "time" integer,
    status integer DEFAULT 0 NOT NULL,
    repost boolean DEFAULT false,
    uid integer DEFAULT 0 NOT NULL
);


ALTER TABLE public.platform_post OWNER TO kiqadmin;

--
-- Name: platform_post_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.platform_post_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.platform_post_id_seq OWNER TO kiqadmin;

--
-- Name: platform_post_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.platform_post_id_seq OWNED BY public.platform_post.id;


--
-- Name: platform_repost; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.platform_repost (
    id integer NOT NULL,
    pid integer NOT NULL,
    r_pid integer NOT NULL
);


ALTER TABLE public.platform_repost OWNER TO kiqadmin;

--
-- Name: platform_repost_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.platform_repost_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.platform_repost_id_seq OWNER TO kiqadmin;

--
-- Name: platform_repost_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.platform_repost_id_seq OWNED BY public.platform_repost.id;


--
-- Name: platform_user; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.platform_user (
    id integer NOT NULL,
    pid integer NOT NULL,
    name text NOT NULL,
    type text
);


ALTER TABLE public.platform_user OWNER TO kiqadmin;

--
-- Name: platform_user_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.platform_user_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.platform_user_id_seq OWNER TO kiqadmin;

--
-- Name: platform_user_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.platform_user_id_seq OWNED BY public.platform_user.id;


--
-- Name: process_result; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.process_result (
    id integer NOT NULL,
    aid integer NOT NULL,
    "time" integer NOT NULL,
    status integer NOT NULL
);


ALTER TABLE public.process_result OWNER TO kiqadmin;

--
-- Name: process_result_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.process_result_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.process_result_id_seq OWNER TO kiqadmin;

--
-- Name: process_result_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.process_result_id_seq OWNED BY public.process_result.id;


--
-- Name: recurring; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.recurring (
    id integer NOT NULL,
    sid integer NOT NULL,
    "time" integer
);


ALTER TABLE public.recurring OWNER TO kiqadmin;

--
-- Name: recurring_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.recurring_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.recurring_id_seq OWNER TO kiqadmin;

--
-- Name: recurring_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.recurring_id_seq OWNED BY public.recurring.id;


--
-- Name: schedule; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.schedule (
    id integer NOT NULL,
    mask integer,
    flags text,
    envfile text,
    "time" integer,
    completed integer DEFAULT 0,
    recurring integer DEFAULT 0,
    notify boolean DEFAULT false,
    runtime text
);


ALTER TABLE public.schedule OWNER TO kiqadmin;

--
-- Name: schedule_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.schedule_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.schedule_id_seq OWNER TO kiqadmin;

--
-- Name: schedule_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.schedule_id_seq OWNED BY public.schedule.id;


--
-- Name: trigger_map; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.trigger_map (
    id integer NOT NULL,
    tid integer NOT NULL,
    old text,
    new text
);


ALTER TABLE public.trigger_map OWNER TO kiqadmin;

--
-- Name: trigger_map_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.trigger_map_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.trigger_map_id_seq OWNER TO kiqadmin;

--
-- Name: trigger_map_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.trigger_map_id_seq OWNED BY public.trigger_map.id;


--
-- Name: triggers; Type: TABLE; Schema: public; Owner: kiqadmin
--

CREATE TABLE public.triggers (
    id integer NOT NULL,
    mask integer,
    trigger_mask integer,
    token_name text,
    token_value text
);


ALTER TABLE public.triggers OWNER TO kiqadmin;

--
-- Name: triggers_id_seq; Type: SEQUENCE; Schema: public; Owner: kiqadmin
--

CREATE SEQUENCE public.triggers_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.triggers_id_seq OWNER TO kiqadmin;

--
-- Name: triggers_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: kiqadmin
--

ALTER SEQUENCE public.triggers_id_seq OWNED BY public.triggers.id;


--
-- Name: apps id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.apps ALTER COLUMN id SET DEFAULT nextval('public.apps_id_seq'::regclass);


--
-- Name: file id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.file ALTER COLUMN id SET DEFAULT nextval('public.file_id_seq'::regclass);


--
-- Name: platform id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform ALTER COLUMN id SET DEFAULT nextval('public.platform_id_seq'::regclass);


--
-- Name: platform_affiliate_user id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_affiliate_user ALTER COLUMN id SET DEFAULT nextval('public.platform_affiliate_user_id_seq'::regclass);


--
-- Name: platform_post id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_post ALTER COLUMN id SET DEFAULT nextval('public.platform_post_id_seq'::regclass);


--
-- Name: platform_repost id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_repost ALTER COLUMN id SET DEFAULT nextval('public.platform_repost_id_seq'::regclass);


--
-- Name: platform_user id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_user ALTER COLUMN id SET DEFAULT nextval('public.platform_user_id_seq'::regclass);


--
-- Name: process_result id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.process_result ALTER COLUMN id SET DEFAULT nextval('public.process_result_id_seq'::regclass);


--
-- Name: recurring id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.recurring ALTER COLUMN id SET DEFAULT nextval('public.recurring_id_seq'::regclass);


--
-- Name: schedule id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.schedule ALTER COLUMN id SET DEFAULT nextval('public.schedule_id_seq'::regclass);


--
-- Name: trigger_map id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.trigger_map ALTER COLUMN id SET DEFAULT nextval('public.trigger_map_id_seq'::regclass);


--
-- Name: triggers id; Type: DEFAULT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.triggers ALTER COLUMN id SET DEFAULT nextval('public.triggers_id_seq'::regclass);


--
-- Name: apps apps_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.apps
    ADD CONSTRAINT apps_pkey PRIMARY KEY (id);


--
-- Name: file file_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.file
    ADD CONSTRAINT file_pkey PRIMARY KEY (id);


--
-- Name: platform_affiliate_user platform_affiliate_user_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_affiliate_user
    ADD CONSTRAINT platform_affiliate_user_pkey PRIMARY KEY (id, uid, a_uid);


--
-- Name: platform platform_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform
    ADD CONSTRAINT platform_pkey PRIMARY KEY (id);


--
-- Name: platform_post platform_post_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_post
    ADD CONSTRAINT platform_post_pkey PRIMARY KEY (id, pid);


--
-- Name: platform_repost platform_repost_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_repost
    ADD CONSTRAINT platform_repost_pkey PRIMARY KEY (id, pid);


--
-- Name: platform_user platform_user_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_user
    ADD CONSTRAINT platform_user_pkey PRIMARY KEY (id, pid, name);


--
-- Name: process_result process_result_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.process_result
    ADD CONSTRAINT process_result_pkey PRIMARY KEY (id, aid);


--
-- Name: recurring recurring_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.recurring
    ADD CONSTRAINT recurring_pkey PRIMARY KEY (id, sid);


--
-- Name: schedule schedule_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.schedule
    ADD CONSTRAINT schedule_pkey PRIMARY KEY (id);


--
-- Name: trigger_map trigger_map_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.trigger_map
    ADD CONSTRAINT trigger_map_pkey PRIMARY KEY (id, tid);


--
-- Name: triggers triggers_pkey; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.triggers
    ADD CONSTRAINT triggers_pkey PRIMARY KEY (id);


--
-- Name: platform_user u_id_const; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_user
    ADD CONSTRAINT u_id_const UNIQUE (id);


--
-- Name: apps unique_mask; Type: CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.apps
    ADD CONSTRAINT unique_mask UNIQUE (mask);


--
-- Name: file file_sid_fkey; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.file
    ADD CONSTRAINT file_sid_fkey FOREIGN KEY (sid) REFERENCES public.schedule(id);


--
-- Name: process_result fk_app; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.process_result
    ADD CONSTRAINT fk_app FOREIGN KEY (aid) REFERENCES public.apps(id);


--
-- Name: platform_post fk_platform; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_post
    ADD CONSTRAINT fk_platform FOREIGN KEY (pid) REFERENCES public.platform(id);


--
-- Name: platform_repost fk_platform; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_repost
    ADD CONSTRAINT fk_platform FOREIGN KEY (pid) REFERENCES public.platform(id);


--
-- Name: platform_post fk_platform_origin; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_post
    ADD CONSTRAINT fk_platform_origin FOREIGN KEY (o_pid) REFERENCES public.platform(id);


--
-- Name: platform_repost fk_platform_repost; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_repost
    ADD CONSTRAINT fk_platform_repost FOREIGN KEY (r_pid) REFERENCES public.platform(id);


--
-- Name: trigger_map fk_trigger; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.trigger_map
    ADD CONSTRAINT fk_trigger FOREIGN KEY (tid) REFERENCES public.triggers(id);


--
-- Name: platform_affiliate_user platform_affiliate_user_a_uid_fkey; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_affiliate_user
    ADD CONSTRAINT platform_affiliate_user_a_uid_fkey FOREIGN KEY (a_uid) REFERENCES public.platform_user(id);


--
-- Name: platform_affiliate_user platform_affiliate_user_uid_fkey; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_affiliate_user
    ADD CONSTRAINT platform_affiliate_user_uid_fkey FOREIGN KEY (uid) REFERENCES public.platform_user(id);


--
-- Name: platform_user platform_user_pid_fkey; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.platform_user
    ADD CONSTRAINT platform_user_pid_fkey FOREIGN KEY (pid) REFERENCES public.platform(id);


--
-- Name: recurring recurring_sid_fkey; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.recurring
    ADD CONSTRAINT recurring_sid_fkey FOREIGN KEY (sid) REFERENCES public.schedule(id);


--
-- Name: triggers triggers_mask_fkey; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.triggers
    ADD CONSTRAINT triggers_mask_fkey FOREIGN KEY (mask) REFERENCES public.apps(mask);


--
-- Name: triggers triggers_trigger_mask_fkey; Type: FK CONSTRAINT; Schema: public; Owner: kiqadmin
--

ALTER TABLE ONLY public.triggers
    ADD CONSTRAINT triggers_trigger_mask_fkey FOREIGN KEY (trigger_mask) REFERENCES public.apps(mask);


--
-- PostgreSQL database dump complete
--

