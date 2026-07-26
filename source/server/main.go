package main

import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"net"
	"net/url"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

type Role string

const (
	RoleAdmin     Role = "admin"
	RoleModerator Role = "moderator"
	RoleGamer     Role = "gamer"
)

type Client struct {
	id        uint64
	name      string
	role      Role
	character int
	world     int
	conn      net.Conn
	writer    *bufio.Writer
	writeMu   sync.Mutex
}

type Hub struct {
	mu             sync.RWMutex
	clients        map[uint64]*Client
	history        []string
	nextID         atomic.Uint64
	adminToken     string
	moderatorToken string
}

func escape(value string) string { return url.QueryEscape(value) }
func unescape(value string) string {
	decoded, err := url.QueryUnescape(value)
	if err != nil {
		return value
	}
	return decoded
}

func sanitize(value string, limit int) string {
	value = strings.TrimSpace(strings.ReplaceAll(strings.ReplaceAll(value, "\t", " "), "\n", " "))
	if len(value) > limit {
		value = value[:limit]
	}
	return value
}

func (client *Client) send(parts ...string) error {
	client.writeMu.Lock()
	defer client.writeMu.Unlock()
	if _, err := client.writer.WriteString(strings.Join(parts, "\t") + "\n"); err != nil {
		return err
	}
	return client.writer.Flush()
}

func (hub *Hub) roleForToken(token string) Role {
	switch {
	case token != "" && token == hub.adminToken:
		return RoleAdmin
	case token != "" && token == hub.moderatorToken:
		return RoleModerator
	default:
		return RoleGamer
	}
}

func (hub *Hub) broadcast(parts ...string) {
	hub.mu.RLock()
	clients := make([]*Client, 0, len(hub.clients))
	for _, client := range hub.clients {
		clients = append(clients, client)
	}
	hub.mu.RUnlock()
	for _, client := range clients {
		_ = client.send(parts...)
	}
}

func (hub *Hub) broadcastRecorded(parts ...string) {
	line := strings.Join(parts, "\t")
	hub.mu.Lock()
	hub.history = append(hub.history, line)
	if len(hub.history) > 50 {
		hub.history = hub.history[len(hub.history)-50:]
	}
	hub.mu.Unlock()
	hub.broadcast(parts...)
}

func (hub *Hub) remove(client *Client) {
	hub.mu.Lock()
	_, existed := hub.clients[client.id]
	if existed {
		delete(hub.clients, client.id)
	}
	hub.mu.Unlock()
	_ = client.conn.Close()
	if existed {
		hub.broadcast("LEAVE", strconv.FormatUint(client.id, 10), escape(client.name))
		log.Printf("%s salió del lobby", client.name)
	}
}

func (hub *Hub) findByName(name string) *Client {
	hub.mu.RLock()
	defer hub.mu.RUnlock()
	for _, client := range hub.clients {
		if strings.EqualFold(client.name, name) {
			return client
		}
	}
	return nil
}

func canModerate(role Role) bool { return role == RoleAdmin || role == RoleModerator }

func (hub *Hub) command(sender *Client, message string) bool {
	fields := strings.Fields(message)
	if len(fields) == 0 || !strings.HasPrefix(fields[0], "/") {
		return false
	}
	switch strings.ToLower(fields[0]) {
	case "/kick":
		if !canModerate(sender.role) || len(fields) < 2 {
			_ = sender.send("ERROR", escape("No tienes permiso o falta el nombre del jugador"))
			return true
		}
		target := hub.findByName(fields[1])
		if target == nil || (target.role == RoleAdmin && sender.role != RoleAdmin) {
			_ = sender.send("ERROR", escape("No se puede expulsar a ese jugador"))
			return true
		}
		_ = target.send("KICK", escape("Fuiste expulsado por "+sender.name))
		hub.remove(target)
		hub.broadcastRecorded("SYSTEM", escape(sender.name+" expulsó a "+target.name))
		return true
	case "/role":
		if sender.role != RoleAdmin || len(fields) < 3 {
			_ = sender.send("ERROR", escape("Solo un administrador puede cambiar roles"))
			return true
		}
		target := hub.findByName(fields[1])
		role := Role(strings.ToLower(fields[2]))
		if target == nil || (role != RoleAdmin && role != RoleModerator && role != RoleGamer) {
			_ = sender.send("ERROR", escape("Jugador o rol no válido"))
			return true
		}
		target.role = role
		hub.broadcast("ROLE", strconv.FormatUint(target.id, 10), string(role))
		hub.broadcastRecorded("SYSTEM", escape(target.name+" ahora tiene el rol "+string(role)))
		return true
	case "/announce":
		if !canModerate(sender.role) {
			_ = sender.send("ERROR", escape("No tienes permiso para anunciar"))
			return true
		}
		announcement := sanitize(strings.TrimSpace(strings.TrimPrefix(message, fields[0])), 300)
		hub.broadcastRecorded("ANNOUNCE", escape(sender.name), string(sender.role), escape(announcement))
		return true
	case "/help":
		_ = sender.send("SYSTEM", escape("Comandos: /help, /kick nombre, /announce texto, /role nombre admin|moderator|gamer"))
		return true
	default:
		_ = sender.send("ERROR", escape("Comando desconocido"))
		return true
	}
}

func (hub *Hub) handle(conn net.Conn) {
	defer conn.Close()
	scanner := bufio.NewScanner(conn)
	scanner.Buffer(make([]byte, 1024), 64*1024)
	if !scanner.Scan() {
		return
	}
	hello := strings.Split(scanner.Text(), "\t")
	if len(hello) < 4 || hello[0] != "HELLO" {
		_, _ = fmt.Fprintln(conn, "ERROR\t"+escape("Se esperaba HELLO"))
		return
	}
	name := sanitize(unescape(hello[1]), 32)
	if name == "" {
		name = "Jugador"
	}
	token := unescape(hello[2])
	character, _ := strconv.Atoi(hello[3])
	world := 0
	if len(hello) >= 5 {
		world, _ = strconv.Atoi(hello[4])
	}
	client := &Client{
		id: hub.nextID.Add(1), name: name, role: hub.roleForToken(token), character: character, world: world,
		conn: conn, writer: bufio.NewWriter(conn),
	}

	hub.mu.Lock()
	existing := make([]*Client, 0, len(hub.clients))
	for _, other := range hub.clients {
		existing = append(existing, other)
	}
	history := append([]string(nil), hub.history...)
	hub.clients[client.id] = client
	hub.mu.Unlock()

	_ = client.send("WELCOME", strconv.FormatUint(client.id, 10), string(client.role))
	for _, other := range existing {
		_ = client.send("JOIN", strconv.FormatUint(other.id, 10), escape(other.name), string(other.role), strconv.Itoa(other.character), strconv.Itoa(other.world))
	}
	for _, line := range history {
		fields := strings.Split(line, "\t")
		_ = client.send(fields...)
	}
	hub.broadcast("JOIN", strconv.FormatUint(client.id, 10), escape(client.name), string(client.role), strconv.Itoa(client.character), strconv.Itoa(client.world))
	hub.broadcastRecorded("SYSTEM", escape(client.name+" entró al lobby como "+string(client.role)))
	log.Printf("%s conectado como %s desde %s", client.name, client.role, conn.RemoteAddr())
	defer hub.remove(client)

	for scanner.Scan() {
		parts := strings.Split(scanner.Text(), "\t")
		if len(parts) == 0 {
			continue
		}
		switch parts[0] {
		case "CHAT":
			if len(parts) < 2 {
				continue
			}
			message := sanitize(unescape(parts[1]), 300)
			if message == "" || hub.command(client, message) {
				continue
			}
			hub.broadcastRecorded("CHAT", strconv.FormatInt(time.Now().Unix(), 10), escape(client.name), string(client.role), escape(message))
		case "MOVE":
			if len(parts) < 9 {
				continue
			}
			// Validate all numeric fields before relaying them.
			valid := true
			for _, value := range parts[1:8] {
				if _, err := strconv.ParseFloat(value, 32); err != nil {
					valid = false
					break
				}
			}
			if !valid {
				continue
			}
			if characterValue, err := strconv.Atoi(parts[8]); err == nil {
				client.character = characterValue
			}
			if len(parts) >= 10 {
				if worldValue, err := strconv.Atoi(parts[9]); err == nil {
					client.world = worldValue
				}
			}
			hub.broadcast("MOVE", strconv.FormatUint(client.id, 10), parts[1], parts[2], parts[3], parts[4], parts[5], parts[6], parts[7], strconv.Itoa(client.character), strconv.Itoa(client.world))
		case "PING":
			_ = client.send("PONG")
		}
	}
}

func main() {
	address := flag.String("listen", "127.0.0.1:7777", "dirección TCP del lobby")
	adminToken := flag.String("admin-token", "admin", "token para el rol administrador")
	moderatorToken := flag.String("moderator-token", "moderator", "token para el rol moderador")
	flag.Parse()

	listener, err := net.Listen("tcp", *address)
	if err != nil {
		log.Fatal(err)
	}
	defer listener.Close()
	hub := &Hub{clients: make(map[uint64]*Client), adminToken: *adminToken, moderatorToken: *moderatorToken}
	log.Printf("Lobby de Mialygopolis escuchando en %s", *address)
	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Printf("error de conexión: %v", err)
			continue
		}
		go hub.handle(conn)
	}
}
